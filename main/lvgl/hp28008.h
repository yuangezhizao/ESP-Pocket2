/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_check.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_XPOWERS_ESP_IDF_NEW_API)
#include "driver/i2c_master.h"
#else
#include "driver/i2c.h"
#endif

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#include "esp_lcd_touch_gt911.h"

/* LCD size */
#define EXAMPLE_LCD_H_RES (240)
#define EXAMPLE_LCD_V_RES (320)

/* LCD settings */
#define EXAMPLE_LCD_SPI_NUM (SPI3_HOST)
#define EXAMPLE_LCD_PIXEL_CLK_HZ (40 * 1000 * 1000)
#define EXAMPLE_LCD_CMD_BITS (8)
#define EXAMPLE_LCD_PARAM_BITS (8)
#define EXAMPLE_LCD_COLOR_SPACE (ESP_LCD_COLOR_SPACE_BGR)
#define EXAMPLE_LCD_BITS_PER_PIXEL (16)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (50)
#define EXAMPLE_LCD_BL_ON_LEVEL (1)

/* LCD pins */
#define EXAMPLE_LCD_GPIO_SCLK (GPIO_NUM_12)
#define EXAMPLE_LCD_GPIO_MOSI (GPIO_NUM_11)
#define EXAMPLE_LCD_GPIO_RST (GPIO_NUM_3)
#define EXAMPLE_LCD_GPIO_DC (GPIO_NUM_9)
#define EXAMPLE_LCD_GPIO_CS (GPIO_NUM_10)
#define EXAMPLE_LCD_GPIO_BL (GPIO_NUM_46)

/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM (0)
#define EXAMPLE_TOUCH_I2C_CLK_HZ (400000)

/* LCD touch pins */
#define EXAMPLE_TOUCH_I2C_SCL (GPIO_NUM_48)
#define EXAMPLE_TOUCH_I2C_SDA (GPIO_NUM_47)
#define EXAMPLE_TOUCH_GPIO_INT (GPIO_NUM_45)
#define EXAMPLE_TOUCH_GPIO_RST (GPIO_NUM_21)

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t app_lcd_init(void);
    esp_err_t app_touch_init(void);
    esp_err_t app_lvgl_init(void);
    void app_main_display(void);

#ifdef __cplusplus
}
#endif
