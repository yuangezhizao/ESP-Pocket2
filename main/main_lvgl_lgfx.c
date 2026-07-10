/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 真机 LVGL-on-LovyanGFX demo 入口：I2C/PMU/RTC 初始化后，用 LGFX 驱动 HP28008 渲染 ui_app。
 * 切换 demo 见 main/CMakeLists.txt 的 SRCS 注释。
 */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "i2c_driver.h"
#include "axp202_port.h"
#include "sensor_bm8563.h"   /* bm8563_init（不依赖 esp_lcd 栈的 hp28008.h） */

#if defined(CONFIG_XPOWERS_ESP_IDF_NEW_API) && !defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
#include "i2c_port.h"
#endif

esp_err_t lvgl_lgfx_hp28008_run(void);   /* 实现于 lvgl_lgfx_hp28008.cpp */

static const char *TAG = "MAIN-LVGL-LGFX";

void app_main(void)
{
#if CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW || CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW || \
    ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && \
     (defined(CONFIG_XPOWERS_ESP_IDF_NEW_API) || defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)))
#if defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
    ESP_ERROR_CHECK(i2c_drv_init());
#elif defined(CONFIG_XPOWERS_ESP_IDF_NEW_API)
    ESP_ERROR_CHECK(i2c_init());
#endif
#endif

    ESP_LOGI(TAG, "I2C initialized successfully");
    ESP_ERROR_CHECK(axp202_init());
    i2c_drv_scan();
    ESP_ERROR_CHECK(bm8563_init());

#endif

    ESP_ERROR_CHECK(lvgl_lgfx_hp28008_run());
}
