/**
 *
 * @license MIT License
 *
 * Copyright (c) 2025 lewis he
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      sensor_bm8563.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @date      2025-06-02
 *
 */
#include "sensor_bm8563.h"

#include "SensorPCF8563.hpp"

static const char *TAG = "PCF8563(BM8563)";

SensorPCF8563 rtc;

// extern uint32_t hal_callback(SensorCommCustomHal::Operation op, void *param1, void *param2);

static bool init_done = false;

esp_err_t bm8563_init()
{
    ESP_LOGI(TAG, "----DRIVER PCF8563(BM8563) ----");

    //* Implemented using read and write callback methods, applicable to other platforms
#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

    uint8_t address = 0x51;

    // * Provide the device address to the callback function
    i2c_drv_device_init(address);

    ESP_LOGI(TAG, "Implemented using read and write callback methods");
    if (rtc.begin(i2c_wr_function))
    {
        ESP_LOGI(TAG, "Initializing PCF8563(BM8563) real-time clock successfully!");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize PCF8563(BM8563) real time clock!");
        return ESP_FAIL;
    }
#endif

    //* Use the built-in esp-idf communication method
#if CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)

    ESP_LOGI(TAG, "Implemented using built-in read and write methods (Use higher version >= 5.0 API)");

    // * Using the new API of esp-idf 5.x, you need to pass the I2C BUS handle,
    // * which is useful when the bus shares multiple devices.
    extern i2c_master_bus_handle_t bus_handle;

    if (rtc.begin(bus_handle))
    {
        ESP_LOGI(TAG, "Initializing PCF8563(BM8563) real-time clock successfully!");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize PCF8563(BM8563) real time clock!");
        return ESP_FAIL;
    }

#else

    ESP_LOGI(TAG, "Implemented using built-in read and write methods (Use lower version < 5.0 API)");
    if (rtc.begin((i2c_port_t)CONFIG_I2C_MASTER_PORT_NUM, CONFIG_SENSOR_SDA, CONFIG_SENSOR_SCL))
    {
        ESP_LOGI(TAG, "Initializing PCF8563(BM8563) real-time clock successfully!");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize PCF8563(BM8563) real time clock!");
        return ESP_FAIL;
    }
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
#endif // CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW

    // Unix tm structure sets the time
    struct tm timeinfo;

    timeinfo.tm_year = 2025 - 1900; // Counting starts from 1900, so subtract 1900 here
    timeinfo.tm_mon = 6 - 1;        // Months start at 0, so you need to subtract 1.
    timeinfo.tm_mday = 2;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = -1;

    rtc.setDateTime(timeinfo);

    init_done = true;

    return ESP_OK;
}

void bm8563_loop()
{
    if (!init_done)
    {
        return;
    }

    char buf[64];
    struct tm timeinfo;

    while (1)
    {
        // Get the time C library structure
        rtc.getDateTime(&timeinfo);

        // Format the output using the strftime function
        // For more formats, please refer to :
        // https://man7.org/linux/man-pages/man3/strftime.3.html
        // size_t written = strftime(buf, 64, "%Y-%m-%d %H:%M:%S %Z %z", &timeinfo);
        size_t written = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z(%z)", &timeinfo);

        if (written != 0)
        {
            ESP_LOGI("RTC", "%s", buf);
        }
        else
        {
            ESP_LOGE("RTC", "strftime failed or buffer too small!");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

// #include "platform/SensorCommCustomHal.hpp"

/**
 * @brief  hal_callback
 * @note   SensorLib hal callback
 * @param  op:  Operation Code
 * @param  *param1:  parameter
 * @param  *param2:  parameter
 * @retval
 */
uint32_t hal_callback(SensorCommCustomHal::Operation op, void *param1, void *param2)
{
    switch (op)
    {
    // Set GPIO mode
    case SensorCommCustomHal::OP_PINMODE:
    {
        uint8_t pin = reinterpret_cast<uintptr_t>(param1);
        uint8_t mode = reinterpret_cast<uintptr_t>(param2);
        gpio_config_t config;
        memset(&config, 0, sizeof(config));
        config.pin_bit_mask = 1ULL << pin;
        switch (mode)
        {
        case INPUT:
            config.mode = GPIO_MODE_INPUT;
            break;
        case OUTPUT:
            config.mode = GPIO_MODE_OUTPUT;
            break;
        }
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&config));
    }
    break;
    // Set GPIO level
    case SensorCommCustomHal::OP_DIGITALWRITE:
    {
        uint8_t pin = reinterpret_cast<uintptr_t>(param1);
        uint8_t level = reinterpret_cast<uintptr_t>(param2);
        gpio_set_level((gpio_num_t)pin, level);
    }
    break;
    // Read GPIO level
    case SensorCommCustomHal::OP_DIGITALREAD:
    {
        uint8_t pin = reinterpret_cast<uintptr_t>(param1);
        return gpio_get_level((gpio_num_t)pin);
    }
    break;
    // Get the current running milliseconds
    case SensorCommCustomHal::OP_MILLIS:
        return (uint32_t)(esp_timer_get_time() / 1000LL);

    // Delay in milliseconds
    case SensorCommCustomHal::OP_DELAY:
    {
        if (param1)
        {
            uint32_t ms = reinterpret_cast<uintptr_t>(param1);
            ets_delay_us((ms % portTICK_PERIOD_MS) * 1000UL);
        }
    }
    break;
    // Delay in microseconds
    case SensorCommCustomHal::OP_DELAYMICROSECONDS:
    {
        uint32_t us = reinterpret_cast<uintptr_t>(param1);
        ets_delay_us(us);
    }
    break;
    default:
        break;
    }
    return 0;
}

#endif
