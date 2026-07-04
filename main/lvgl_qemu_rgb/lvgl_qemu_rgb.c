/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 移植自官方例程 lcd_qemu_rgb_panel，将 LVGL v8 裸 lv_disp_drv API 适配为项目使用的 v9。
 */
#include <stdlib.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_qemu_rgb.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "lvgl.h"

#include "lvgl_qemu_rgb.h"

static const char *TAG = "qemu_rgb";

/* QEMU 虚拟 RGB 面板尺寸（可自定义；沿用官方默认） */
#define QEMU_LCD_H_RES              (800)
#define QEMU_LCD_V_RES              (480)
/* 分离式 draw buffer 行数（内部 RAM） */
#define QEMU_LVGL_BUF_LINES         (10)

/* 色深 -> QEMU bpp / LVGL color format / 每像素字节（对齐官方，v9 转译） */
#if CONFIG_LV_COLOR_DEPTH_32
#define QEMU_RGB_BPP            RGB_QEMU_BPP_32
#define QEMU_LVGL_COLOR_FORMAT  LV_COLOR_FORMAT_XRGB8888
#define QEMU_LVGL_COLOR_FORMAT_NAME "XRGB8888"
#define QEMU_LVGL_BYTES_PER_PX  4
#elif CONFIG_LV_COLOR_DEPTH_16
#define QEMU_RGB_BPP            RGB_QEMU_BPP_16
#define QEMU_LVGL_COLOR_FORMAT  LV_COLOR_FORMAT_RGB565
#define QEMU_LVGL_COLOR_FORMAT_NAME "RGB565"
#define QEMU_LVGL_BYTES_PER_PX  2
#else
#error "QEMU RGB Panel only supports 16-bit and 32-bit color depth, please set LV_COLOR_DEPTH to 16 or 32"
#endif

/* 诊断：PSRAM 编译开关字符串（供日志与 GUI 自证） */
#if CONFIG_SPIRAM
#define QEMU_PSRAM_CFG "PSRAM=on"
#else
#define QEMU_PSRAM_CFG "PSRAM=off"
#endif

/* 诊断：本次实际生效配置字符串，由 setup_buffers 填充，供 GUI 读取 */
static char s_qemu_rgb_diag[96];

#define QEMU_LVGL_TICK_PERIOD_MS    (2)
#define QEMU_LVGL_TASK_MAX_DELAY_MS (500)
#define QEMU_LVGL_TASK_MIN_DELAY_MS (1)
#define QEMU_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define QEMU_LVGL_TASK_PRIORITY     (2)

static SemaphoreHandle_t s_lvgl_mux = NULL;

static void qemu_rgb_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    /* QEMU 面板直接写虚拟帧缓冲，无需字节交换（区别于给 SPI 用的 swap_bytes） */
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    if (err != ESP_OK) {
        /* 记录但不中断：仍须 lv_display_flush_ready 让 LVGL 继续，否则会一直等 buffer 就绪而停摆 */
        ESP_LOGW(TAG, "draw_bitmap failed (%s), frame skipped", esp_err_to_name(err));
    }
    lv_display_flush_ready(disp);
}

static void qemu_rgb_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(QEMU_LVGL_TICK_PERIOD_MS);
}

static bool qemu_rgb_lvgl_lock(int timeout_ms)
{
    const TickType_t ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mux, ticks) == pdTRUE;
}

static void qemu_rgb_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mux);
}

static void qemu_rgb_lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = QEMU_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        if (qemu_rgb_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            qemu_rgb_lvgl_unlock();
        }
        if (task_delay_ms > QEMU_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = QEMU_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < QEMU_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = QEMU_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static esp_err_t qemu_rgb_lvgl_setup_buffers(lv_display_t *disp, esp_lcd_panel_handle_t panel)
{
    void *buf1 = NULL;
    const char *mode;
#if CONFIG_LVGL_QEMU_RGB_DEDIC_FB
    ESP_LOGI(TAG, "Use QEMU dedicated frame buffer as LVGL draw buffer");
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_qemu_get_frame_buffer(panel, &buf1), TAG, "get frame buffer failed");
    const size_t buf_size = QEMU_LCD_H_RES * QEMU_LCD_V_RES * QEMU_LVGL_BYTES_PER_PX; /* 整屏 */
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    mode = "DEDIC_FB/FULL";
#else
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffer");
    const size_t buf_size = QEMU_LCD_H_RES * QEMU_LVGL_BUF_LINES * QEMU_LVGL_BYTES_PER_PX;
    buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf1, ESP_ERR_NO_MEM, TAG, "alloc LVGL draw buffer failed");
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    mode = "PARTIAL";
#endif
    /* 诊断：记录实际生效配置，供日志与 GUI 自证（render mode / buffer 地址 / 内存类型 / 色深 / PSRAM 开关） */
    const char *mem = esp_ptr_external_ram(buf1) ? "PSRAM"
                      : (esp_ptr_internal(buf1) ? "INTERNAL" : "OTHER");
    snprintf(s_qemu_rgb_diag, sizeof(s_qemu_rgb_diag), "%s | buf@%p %s | %s | %s",
             mode, buf1, mem, QEMU_LVGL_COLOR_FORMAT_NAME, QEMU_PSRAM_CFG);
    ESP_LOGW(TAG, "[DIAG] %s", s_qemu_rgb_diag);
    return ESP_OK;
}

/* 诊断：供 UI 展示本次实际生效配置 */
const char *qemu_rgb_diag_str(void)
{
    return s_qemu_rgb_diag;
}

esp_err_t qemu_rgb_lvgl_run(void)
{
    ESP_LOGI(TAG, "Install QEMU RGB LCD panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_rgb_qemu_config_t panel_config = {
        .width = QEMU_LCD_H_RES,
        .height = QEMU_LCD_V_RES,
        .bpp = QEMU_RGB_BPP,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_qemu(&panel_config, &panel_handle), TAG,
                        "esp_lcd_new_rgb_qemu failed (this example only runs in QEMU)");

    ESP_LOGI(TAG, "Initialize QEMU RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lv_display_t *disp = lv_display_create(QEMU_LCD_H_RES, QEMU_LCD_V_RES);
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_NO_MEM, TAG, "lv_display_create failed");
    lv_display_set_color_format(disp, QEMU_LVGL_COLOR_FORMAT);
    lv_display_set_user_data(disp, panel_handle);
    lv_display_set_flush_cb(disp, qemu_rgb_lvgl_flush_cb);

    ESP_RETURN_ON_ERROR(qemu_rgb_lvgl_setup_buffers(disp, panel_handle), TAG, "setup buffers failed");

    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &qemu_rgb_increase_lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, QEMU_LVGL_TICK_PERIOD_MS * 1000));

    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    ESP_RETURN_ON_FALSE(s_lvgl_mux, ESP_ERR_NO_MEM, TAG, "create LVGL mutex failed");

    ESP_LOGI(TAG, "Create LVGL task");
    BaseType_t task_ok = xTaskCreate(qemu_rgb_lvgl_task, "LVGL", QEMU_LVGL_TASK_STACK_SIZE, NULL, QEMU_LVGL_TASK_PRIORITY, NULL);
    ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_ERR_NO_MEM, TAG, "create LVGL task failed");

    ESP_LOGI(TAG, "Display LVGL demo UI");
    if (qemu_rgb_lvgl_lock(-1)) {
        qemu_rgb_lvgl_demo_ui(disp);
        qemu_rgb_lvgl_unlock();
    }
    return ESP_OK;
}
