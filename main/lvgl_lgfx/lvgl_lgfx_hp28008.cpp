/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 真机适配：LGFX 只做显示（不 setTouch），触摸走 esp_lcd_touch_gt911（复用项目 bus_handle），
 * 时钟走 BM8563 RTC；自管 LVGL 运行时（esp_timer tick + FreeRTOS task + recursive mutex）。
 * 不改动 main/lvgl/hp28008.c。
 */
#include "lvgl_lgfx_bridge.hpp"
#include "lgfx_hp28008.hpp"

/* 各头自带 extern "C" 边界，C++ 文件普通 include 即可，切勿外包 extern "C"：
 * i2c_app.h 在 __cplusplus 下会引入纯 C++ 的 RtcDrv.hpp/rtc，且 RTC_global_variable 在 extern "C" 外，
 * 外包会把它们拖入 C linkage → 编译失败(namespace/STL) 或链接冲突。现有 i2c_app.cpp 亦是普通 include。 */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"   /* ESP_RETURN_ON_ERROR / ESP_RETURN_ON_FALSE */
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "i2c_app.h"   /* update_RTC_global_variable / RTC_global_variable */

/* 本入口依赖项目 i2c_driver 的 bus_handle（GT911 复用），该符号仅在 SensorLib 新 API 下定义。
 * 注意：main 组件对各子目录是「全量 GLOB 编译」（见 main/CMakeLists.txt），本文件无论当前生效入口
 * 是谁都会被编译；故不能用顶层 #error（否则 old API 配置会波及所有入口的构建），改为：新 API 下编译
 * 完整实现，old API 下编译一个返回 ESP_ERR_NOT_SUPPORTED 的 stub（见文件末尾 #else 分支）。 */
#if defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)

static const char *TAG = "LVGL-LGFX-HP28008";

#define LVGL_LGFX_TICK_PERIOD_MS   2
#define LVGL_LGFX_TASK_STACK_SIZE  (8 * 1024)
#define LVGL_LGFX_TASK_PRIORITY    4
#define LVGL_LGFX_TASK_MAX_DELAY   500
#define LVGL_LGFX_TASK_MIN_DELAY   1
#define LVGL_LGFX_TOUCH_RST_GPIO   GPIO_NUM_21
#define LVGL_LGFX_TOUCH_INT_GPIO   GPIO_NUM_45
#define LVGL_LGFX_TOUCH_CLK_HZ     400000
#define LVGL_LGFX_BL_BRIGHTNESS    128   /* 0..255，约 50% */

/* 必须 with_touch=false：真机 GT911 走 esp_lcd_touch/项目 bus，LGFX 绝不能再 init I2C，
 * 否则重现 spec 第 3 节的新旧驱动同总线冲突。切勿改用默认构造。 */
static LGFX_HP28008 s_lcd(false);
static esp_lcd_touch_handle_t s_touch = NULL;
static SemaphoreHandle_t s_lvgl_mux = NULL;

/* 触摸源：读 esp_lcd_touch 的原生（rotation-0）物理坐标并**原样回传**；旋转由 LVGL 统一负责
 * （见下方实现注释与 spec 第 8.1/D7）。物理方向基线校准见 run() 里 tp_cfg 的 esp_lcd_touch flags。 */
static bool real_touch_read(int32_t *x, int32_t *y)
{
    /* esp_lcd_touch 1.2.1 起 deprecate get_coordinates 并新增 get_data（返回 esp_err_t，触点数看 cnt）；
     * 本 plan 已在 idf_component.yml 约束底层 esp_lcd_touch ^1.2.1 保证 get_data 可用（见 Task 3 Step 3）。
     * esp_lcd_touch_point_data_t 的字段(.x/.y)以实际拉取的组件头为准（见验证策略「API 核对」）。 */
    if (esp_lcd_touch_read_data(s_touch) != ESP_OK) {
        return false;   /* I2C 瞬时读失败：本帧视为无触摸、下帧重试，好过沿用内部旧数据 */
    }
    esp_lcd_touch_point_data_t points[1] = {};
    uint8_t cnt = 0;
    if (esp_lcd_touch_get_data(s_touch, points, &cnt, 1) != ESP_OK || cnt == 0) {
        return false;
    }
    /* 回传原生（rotation-0）物理坐标；旋转统一由 LVGL 的 indev_pointer_proc→lv_display_rotate_point
     * 负责（LVGL v9 对 read_cb 坐标无条件再旋转，期望 read_cb 回传原生坐标）。切勿在此按旋转手动变换，
     * 否则与 LVGL 的旋转叠加成「双重旋转」（rotation 非 0 时触点错位/越界，见 spec 第 8.1/D7）。
     * 物理方向基线（左右/上下/交换）若真机实测不对，改用下方 tp_cfg 的 esp_lcd_touch flags 校准，而非在此改。 */
    /* 触摸基线调试：取消下一行注释，可在串口打印 GT911 原生(rotation-0)坐标，用于确认触点落在哪个角、
     * 从而校准物理基线——竖屏下依次点四角，左上应≈(0,0)、右上≈(W-1,0)、左下≈(0,H-1)、右下≈(W-1,H-1)；
     * 若点左上处 x 偏大→需 mirror_x，y 偏大→需 mirror_y，x/y 量程互换→需 swap_xy（改下方 tp_cfg.flags）。
     * 本板已实测四角象限/量程均正确，故默认注释、flags 保持全 0（见下方 tp_cfg 注释与 spec 第 9 节）。 */
    // ESP_LOGI(TAG, "touch raw x=%d y=%d", (int)points[0].x, (int)points[0].y);
    *x = points[0].x;
    *y = points[0].y;
    return true;
}

/* 时钟源：读 BM8563 RTC（不改 hp28008.c，此处独立定义；刷新失败返回 NULL） */
static const char *real_clock_source(void)
{
    return update_RTC_global_variable() ? RTC_global_variable : NULL;
}

static void lvgl_tick_cb(void *arg)
{
    LV_UNUSED(arg);
    lv_tick_inc(LVGL_LGFX_TICK_PERIOD_MS);
}

static void lvgl_task(void *arg)
{
    LV_UNUSED(arg);
    uint32_t delay_ms = LVGL_LGFX_TASK_MAX_DELAY;
    while (1) {
        if (xSemaphoreTakeRecursive(s_lvgl_mux, portMAX_DELAY) == pdTRUE) {
            delay_ms = lv_timer_handler();
            xSemaphoreGiveRecursive(s_lvgl_mux);
        }
        if (delay_ms > LVGL_LGFX_TASK_MAX_DELAY) delay_ms = LVGL_LGFX_TASK_MAX_DELAY;
        else if (delay_ms < LVGL_LGFX_TASK_MIN_DELAY) delay_ms = LVGL_LGFX_TASK_MIN_DELAY;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

extern "C" esp_err_t lvgl_lgfx_hp28008_run(void)
{
    ESP_LOGI(TAG, "Init LGFX display (ST7789 SPI, display-only)");
    /* LGFX_Device::init() 返回 bool；ST7789(Panel_LCD) 实际恒真，此检查为防御性/与本文件其他初始化一致 */
    ESP_RETURN_ON_FALSE(s_lcd.init(), ESP_FAIL, TAG, "LGFX init failed");
    /* 显式 16bpp RGB565：与 bridge 的 (rgb565_t*) flush、spec 色深前提及 PC 侧 setColorDepth(16) 对称。
     * LGFX Panel 默认 _write_depth 虽即 rgb565_2Byte，仍显式声明，不留隐式默认值依赖。 */
    s_lcd.setColorDepth(16);
    s_lcd.setBrightness(LVGL_LGFX_BL_BRIGHTNESS);

    ESP_LOGI(TAG, "Init GT911 touch via esp_lcd_touch (reuse project bus_handle)");
    extern i2c_master_bus_handle_t bus_handle;   /* 由 main_lvgl_lgfx.c 的 i2c_drv_init 建立 */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    /* 逐字段赋值而非 ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG() 宏：该宏内部是「乱序」designated-init
     * (.scl_speed_hz 排在 .dev_addr 前，而结构体声明 dev_addr 在前/scl_speed_hz 在末)，C 合法但
     * C++(-std=gnu++2b) 判定 designator-order 硬错误（hp28008.c 是 C 文件故无此问题、无法沿用宏）。
     * 故先 {} 清零再逐字段赋值（与本文件 tp_cfg/tick_args 同法），等价复刻宏所设的全部字段。 */
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = {};
    tp_io_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
    tp_io_cfg.control_phase_bytes = 1;
    tp_io_cfg.dc_bit_offset = 0;
    tp_io_cfg.lcd_cmd_bits = 16;
    tp_io_cfg.flags.disable_control_phase = 1;
    tp_io_cfg.scl_speed_hz = LVGL_LGFX_TOUCH_CLK_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus_handle, &tp_io_cfg, &tp_io), TAG, "touch io failed");

    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = UI_APP_HOR_RES;
    tp_cfg.y_max = UI_APP_VER_RES;
    tp_cfg.rst_gpio_num = LVGL_LGFX_TOUCH_RST_GPIO;
    tp_cfg.int_gpio_num = LVGL_LGFX_TOUCH_INT_GPIO;
    tp_cfg.levels.reset = 0;
    tp_cfg.levels.interrupt = 0;
    /* 物理方向基线（rotation-0，GT911 原始坐标→面板坐标的交换/镜像）：真机已实测四角象限/量程正确
     * （左上≈(33,46)/右上≈(225,40)/左下≈(5,317)/右下≈(227,316)：x 随左→右增、y 随上→下增、量程各匹配
     * 屏宽/高），故三 flag 全 0（不变换）；旋转由 LVGL 负责（见 real_touch_read 的调试 log）。换屏后基线不同再改。 */
    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 0;
    tp_cfg.flags.mirror_y = 0;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_touch), TAG, "gt911 failed");

    ESP_LOGI(TAG, "Init LVGL + bridge");
    lv_init();
    const uint32_t buf_bytes = UI_APP_HOR_RES * LVGL_LGFX_DRAW_BUFF_HEIGHT * sizeof(lgfx::rgb565_t);
    void *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    void *buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buf1 && buf2, ESP_ERR_NO_MEM, TAG, "draw buffer alloc failed");
    /* demo 一次性初始化：任一步失败即返回 esp_err_t 由上层 ESP_ERROR_CHECK 触发重启，
     * 故未对已分配的 buf1/tp_io 做回收（进程重启释放），可接受。 */

    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    ESP_RETURN_ON_FALSE(s_lvgl_mux, ESP_ERR_NO_MEM, TAG, "create mutex failed");

    xSemaphoreTakeRecursive(s_lvgl_mux, portMAX_DELAY);
    lvgl_lgfx_bridge_cfg_t cfg = {};
    cfg.lcd = &s_lcd;
    cfg.buf1 = buf1;
    cfg.buf2 = buf2;
    cfg.buf_size_bytes = buf_bytes;
    cfg.touch_read = real_touch_read;
    cfg.clock_src = real_clock_source;
    lv_display_t *disp = lvgl_lgfx_bridge_create(&cfg);
    xSemaphoreGiveRecursive(s_lvgl_mux);
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_NO_MEM, TAG, "bridge create failed");

    esp_timer_create_args_t tick_args = {};   /* 逐字段赋值，与上面 tp_cfg 风格统一，避免 C++ designated-init 顺序问题 */
    tick_args.callback = &lvgl_tick_cb;
    tick_args.name = "lvgl_lgfx_tick";
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_LGFX_TICK_PERIOD_MS * 1000));

    BaseType_t ok = xTaskCreate(lvgl_task, "lvgl_lgfx", LVGL_LGFX_TASK_STACK_SIZE, NULL,
                                LVGL_LGFX_TASK_PRIORITY, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "create lvgl task failed");
    return ESP_OK;
}

#else  /* !CONFIG_SENSORLIB_ESP_IDF_NEW_API */

/* old API 配置：无 i2c_driver 的 bus_handle，无法复用总线驱动 GT911；导出 stub 保证 main 组件在 SensorLib
 * 新旧 API 任一配置下都能编译（不破坏全量 GLOB 下的其它入口），仅当真把生效入口切到 main_lvgl_lgfx 才在运行时返回不支持。 */
extern "C" esp_err_t lvgl_lgfx_hp28008_run(void)
{
    ESP_LOGE("LVGL-LGFX-HP28008",
             "main_lvgl_lgfx requires CONFIG_SENSORLIB_ESP_IDF_NEW_API=y to reuse i2c_driver bus_handle for GT911");
    return ESP_ERR_NOT_SUPPORTED;
}

#endif  /* CONFIG_SENSORLIB_ESP_IDF_NEW_API */
