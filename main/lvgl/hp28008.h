/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <inttypes.h>

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
#include "driver/ledc.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#include "esp_lcd_touch_gt911.h"

/* LCD size */
#define EXAMPLE_LCD_H_RES (240) // Note: DO NOT CHANGE
#define EXAMPLE_LCD_V_RES (320) // Note: DO NOT CHANGE

/* LCD settings */
#define EXAMPLE_LCD_SPI_NUM (SPI3_HOST)             // Note: (SPI2_HOST) in spi_lcd_touch_example_main.c#L38
#define EXAMPLE_LCD_PIXEL_CLK_HZ (40 * 1000 * 1000) // Note: (20 * 1000 * 1000) in spi_lcd_touch_example_main.c#L43
#define EXAMPLE_LCD_CMD_BITS (8)
#define EXAMPLE_LCD_PARAM_BITS (8)
#define EXAMPLE_LCD_COLOR_SPACE (ESP_LCD_COLOR_SPACE_RGB)
#define EXAMPLE_LCD_BITS_PER_PIXEL (16)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (50) // Note: 20 in spi_lcd_touch_example_main.c#L67

#define EXAMPLE_LCD_BL_USE_LEDC (1)
#define EXAMPLE_LCD_BL_ON_LEVEL (1)
#define EXAMPLE_LCD_DIRECTION (0)
#define EXAMPLE_LCD_INVERT_COLOR (1)

// #if EXAMPLE_LCD_DIRECTION == (0)
// #define EXAMPLE_LCD_DIRECTION_SWAP_X_Y (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_X (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_Y (0)
// #elif EXAMPLE_LCD_DIRECTION == (90)
// #define EXAMPLE_LCD_DIRECTION_SWAP_X_Y (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_X (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_Y (0)
// #elif EXAMPLE_LCD_DIRECTION == (180)
// #define EXAMPLE_LCD_DIRECTION_SWAP_X_Y (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_X (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_Y (0)
// #elif EXAMPLE_LCD_DIRECTION == (270)
// #define EXAMPLE_LCD_DIRECTION_SWAP_X_Y (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_X (0)
// #define EXAMPLE_LCD_DIRECTION_MIRROR_Y (0)
// #endif

/* LCD pins */
#define EXAMPLE_LCD_GPIO_SCLK (GPIO_NUM_12)
#define EXAMPLE_LCD_GPIO_MOSI (GPIO_NUM_11)
#define EXAMPLE_LCD_GPIO_MISO (GPIO_NUM_13)
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

/* LEDC settings */
/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2 (rev < 1.2), ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO EXAMPLE_LCD_GPIO_BL // Define the output GPIO
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
// #define LEDC_DUTY (4096)                // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_DUTY_MAX (8192)  // (2 ** 13) = 8192
#define LEDC_FREQUENCY (4000) // Frequency in Hertz. Set frequency at 4 kHz

/* LVGL settings */
#define EXAMPLE_LVGL_TASK_PRIORITY 4 // Note: 2 in spi_lcd_touch_example_main.c#L72
#define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)
#define EXAMPLE_LVGL_TASK_AFFINITY -1
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TICK_PERIOD_MS 5 // Note: 2 in spi_lcd_touch_example_main.c#L68

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
