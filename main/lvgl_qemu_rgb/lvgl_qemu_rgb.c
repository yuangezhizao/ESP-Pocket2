/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 移植自官方例程 lcd_qemu_rgb_panel，将 LVGL v8 裸 lv_disp_drv API 适配为项目使用的 v9。
 */
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_qemu_rgb.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "lvgl_qemu_rgb.h"

static const char *TAG = "qemu_rgb";

/* QEMU 虚拟 RGB 面板尺寸（可自定义；沿用官方默认） */
#define QEMU_LCD_H_RES              (800)
#define QEMU_LCD_V_RES              (480)
/* 分离式 draw buffer 行数（内部 RAM） */
#define QEMU_LVGL_BUF_LINES         (10)

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
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
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

esp_err_t qemu_rgb_lvgl_run(void)
{
    ESP_LOGI(TAG, "Install QEMU RGB LCD panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_rgb_qemu_config_t panel_config = {
        .width = QEMU_LCD_H_RES,
        .height = QEMU_LCD_V_RES,
        .bpp = RGB_QEMU_BPP_16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_qemu(&panel_config, &panel_handle), TAG,
                        "esp_lcd_new_rgb_qemu failed (this example only runs in QEMU)");

    ESP_LOGI(TAG, "Initialize QEMU RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lv_display_t *disp = lv_display_create(QEMU_LCD_H_RES, QEMU_LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, panel_handle);
    lv_display_set_flush_cb(disp, qemu_rgb_lvgl_flush_cb);

    ESP_LOGI(TAG, "Allocate separate LVGL draw buffer");
    const size_t buf_size = QEMU_LCD_H_RES * QEMU_LVGL_BUF_LINES * sizeof(lv_color16_t); /* RGB565 = 2B/px */
    void *buf1 = malloc(buf_size);
    assert(buf1);
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &qemu_rgb_increase_lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, QEMU_LVGL_TICK_PERIOD_MS * 1000));

    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(s_lvgl_mux);

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(qemu_rgb_lvgl_task, "LVGL", QEMU_LVGL_TASK_STACK_SIZE, NULL, QEMU_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL demo UI");
    if (qemu_rgb_lvgl_lock(-1)) {
        qemu_rgb_lvgl_demo_ui(disp);
        qemu_rgb_lvgl_unlock();
    }
    return ESP_OK;
}
