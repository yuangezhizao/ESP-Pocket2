/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * HP28008（ST7789 SPI 240x320 + GT911）的 LovyanGFX 设备类。
 * with_touch=true（默认）供 lgfx_test() 用；with_touch=false 供真机 LVGL 入口用
 * （不 setTouch → LGFX 不碰 I2C，触摸改走 esp_lcd_touch，见 spec 第 3 节 I2C 冲突约束）。
 * 引脚与 main/lvgl/hp28008.h 的 EXAMPLE_LCD_* / EXAMPLE_TOUCH_* 约定同值（唯 touch I2C port 例外，见 HP28008_LGFX_TOUCH_I2C_PORT 处说明）。
 */
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define HP28008_LGFX_SPI_HOST       SPI3_HOST
#define HP28008_LGFX_PIN_SCLK       GPIO_NUM_12
#define HP28008_LGFX_PIN_MOSI       GPIO_NUM_11
#define HP28008_LGFX_PIN_MISO       GPIO_NUM_13
#define HP28008_LGFX_PIN_DC         GPIO_NUM_9
#define HP28008_LGFX_PIN_CS         GPIO_NUM_10
#define HP28008_LGFX_PIN_RST        GPIO_NUM_3
#define HP28008_LGFX_PIN_BL         GPIO_NUM_46
/* 面板尺寸：本头保持零 LVGL 依赖（供纯 LovyanGFX 的 lgfx_test 复用），故独立定义；
 * 与 ui_app.h 的 UI_APP_HOR_RES/VER_RES、hp28008.h 的 EXAMPLE_LCD_H_RES/V_RES 三处约定同值(240x320)。 */
#define HP28008_LGFX_PANEL_W        240
#define HP28008_LGFX_PANEL_H        320
#define HP28008_LGFX_SPI_FREQ_WRITE 40000000
#define HP28008_LGFX_SPI_FREQ_READ  16000000
#define HP28008_LGFX_BL_PWM_CH      7
#define HP28008_LGFX_BL_PWM_FREQ    44100
#define HP28008_LGFX_TOUCH_SDA      GPIO_NUM_47
#define HP28008_LGFX_TOUCH_SCL      GPIO_NUM_48
#define HP28008_LGFX_TOUCH_INT      GPIO_NUM_45
/* touch I2C port 沿用现有 lgfx.cpp 的 1（仅 with_touch=true 的 lgfx_test 用；真机 LVGL 入口 with_touch=false，
 * 触摸走 esp_lcd_touch 的项目 bus=port 0，不经此）。故此项与 hp28008.h 的 EXAMPLE_TOUCH_I2C_NUM=0 不同值；
 * 其余触摸引脚(SDA/SCL/INT/ADDR/FREQ)与 hp28008.h 同值。 */
#define HP28008_LGFX_TOUCH_I2C_PORT 1
#define HP28008_LGFX_TOUCH_ADDR     0x5D
#define HP28008_LGFX_TOUCH_FREQ     400000

class LGFX_HP28008 : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
    lgfx::Light_PWM    _light_instance;
    lgfx::Touch_GT911  _touch_instance;

public:
    explicit LGFX_HP28008(bool with_touch = true)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = HP28008_LGFX_SPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = HP28008_LGFX_SPI_FREQ_WRITE;
            cfg.freq_read = HP28008_LGFX_SPI_FREQ_READ;
            cfg.spi_3wire = true;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = HP28008_LGFX_PIN_SCLK;
            cfg.pin_mosi = HP28008_LGFX_PIN_MOSI;
            cfg.pin_miso = HP28008_LGFX_PIN_MISO;
            cfg.pin_dc = HP28008_LGFX_PIN_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = HP28008_LGFX_PIN_CS;
            cfg.pin_rst = HP28008_LGFX_PIN_RST;
            cfg.pin_busy = GPIO_NUM_NC;
            cfg.panel_width = HP28008_LGFX_PANEL_W;
            cfg.panel_height = HP28008_LGFX_PANEL_H;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            /* invert=false 沿用现有 lgfx.cpp 的 LovyanGFX 配置（其 ST7789 默认极性与 esp_lcd 不同）；
             * 与 hp28008.h 的 EXAMPLE_LCD_INVERT_COLOR=1 语义不同源、勿直接对齐，真机若颜色反相再调。 */
            cfg.invert = false;
            cfg.rgb_order = true;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = HP28008_LGFX_PIN_BL;
            cfg.invert = false;
            cfg.freq = HP28008_LGFX_BL_PWM_FREQ;
            cfg.pwm_channel = HP28008_LGFX_BL_PWM_CH;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        if (with_touch)
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = HP28008_LGFX_PANEL_W - 1;
            cfg.y_min = 0;
            cfg.y_max = HP28008_LGFX_PANEL_H - 1;
            cfg.pin_int = HP28008_LGFX_TOUCH_INT;
            cfg.bus_shared = true;
            cfg.offset_rotation = 0;
            cfg.i2c_port = HP28008_LGFX_TOUCH_I2C_PORT;
            cfg.i2c_addr = HP28008_LGFX_TOUCH_ADDR;
            cfg.pin_sda = HP28008_LGFX_TOUCH_SDA;
            cfg.pin_scl = HP28008_LGFX_TOUCH_SCL;
            cfg.freq = HP28008_LGFX_TOUCH_FREQ;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};
