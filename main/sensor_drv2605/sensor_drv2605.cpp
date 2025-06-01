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
 * @file      sensor_drv2605.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @date      2025-06-01
 *
 */
#include "sensor_drv2605.h"

#include "SensorDRV2605.hpp"

static const char *TAG = "DRV2605";

#define DRV2605_I2C_ADDRESS 0x5A
#define MAX_EFFECT_ID 123

SensorDRV2605 drv;

// extern uint32_t hal_callback(SensorCommCustomHal::Operation op, void *param1, void *param2);

static bool init_done = false;

uint8_t effect = 1;

const char *effect_descriptions[MAX_EFFECT_ID + 1] = {
    "",
    "1 - Strong Click - 100%",
    "2 - Strong Click - 60%",
    "3 - Strong Click - 30%",
    "4 - Sharp Click - 100%",
    "5 - Sharp Click - 60%",
    "6 - Sharp Click - 30%",
    "7 - Soft Bump - 100%",
    "8 - Soft Bump - 60%",
    "9 - Soft Bump - 30%",
    "10 - Double Click - 100%",
    "11 - Double Click - 60%",
    "12 - Triple Click - 100%",
    "13 - Soft Fuzz - 60%",
    "14 - Strong Buzz - 100%",
    "15 - 750 ms Alert 100%",
    "16 - 1000 ms Alert 100%",
    "17 - Strong Click 1 - 100%",
    "18 - Strong Click 2 - 80%",
    "19 - Strong Click 3 - 60%",
    "20 - Strong Click 4 - 30%",
    "21 - Medium Click 1 - 100%",
    "22 - Medium Click 2 - 80%",
    "23 - Medium Click 3 - 60%",
    "24 - Sharp Tick 1 - 100%",
    "25 - Sharp Tick 2 - 80%",
    "26 - Sharp Tick 3 - 60%",
    "27 - Short Double Click Strong 1 - 100%",
    "28 - Short Double Click Strong 2 - 80%",
    "29 - Short Double Click Strong 3 - 60%",
    "30 - Short Double Click Strong 4 - 30%",
    "31 - Short Double Click Medium 1 - 100%",
    "32 - Short Double Click Medium 2 - 80%",
    "33 - Short Double Click Medium 3 - 60%",
    "34 - Short Double Sharp Tick 1 - 100%",
    "35 - Short Double Sharp Tick 2 - 80%",
    "36 - Short Double Sharp Tick 3 - 60%",
    "37 - Long Double Sharp Click Strong 1 - 100%",
    "38 - Long Double Sharp Click Strong 2 - 80%",
    "39 - Long Double Sharp Click Strong 3 - 60%",
    "40 - Long Double Sharp Click Strong 4 - 30%",
    "41 - Long Double Sharp Click Medium 1 - 100%",
    "42 - Long Double Sharp Click Medium 2 - 80%",
    "43 - Long Double Sharp Click Medium 3 - 60%",
    "44 - Long Double Sharp Tick 1 - 100%",
    "45 - Long Double Sharp Tick 2 - 80%",
    "46 - Long Double Sharp Tick 3 - 60%",
    "47 - Buzz 1 - 100%",
    "48 - Buzz 2 - 80%",
    "49 - Buzz 3 - 60%",
    "50 - Buzz 4 - 40%",
    "51 - Buzz 5 - 20%",
    "52 - Pulsing Strong 1 - 100%",
    "53 - Pulsing Strong 2 - 60%",
    "54 - Pulsing Medium 1 - 100%",
    "55 - Pulsing Medium 2 - 60%",
    "56 - Pulsing Sharp 1 - 100%",
    "57 - Pulsing Sharp 2 - 60%",
    "58 - Transition Click 1 - 100%",
    "59 - Transition Click 2 - 80%",
    "60 - Transition Click 3 - 60%",
    "61 - Transition Click 4 - 40%",
    "62 - Transition Click 5 - 20%",
    "63 - Transition Click 6 - 10%",
    "64 - Transition Hum 1 - 100%",
    "65 - Transition Hum 2 - 80%",
    "66 - Transition Hum 3 - 60%",
    "67 - Transition Hum 4 - 40%",
    "68 - Transition Hum 5 - 20%",
    "69 - Transition Hum 6 - 10%",
    "70 - Transition Ramp Down Long Smooth 1 - 100 to 0%",
    "71 - Transition Ramp Down Long Smooth 2 - 100 to 0%",
    "72 - Transition Ramp Down Medium Smooth 1 - 100 to 0%",
    "73 - Transition Ramp Down Medium Smooth 2 - 100 to 0%",
    "74 - Transition Ramp Down Short Smooth 1 - 100 to 0%",
    "75 - Transition Ramp Down Short Smooth 2 - 100 to 0%",
    "76 - Transition Ramp Down Long Sharp 1 - 100 to 0%",
    "77 - Transition Ramp Down Long Sharp 2 - 100 to 0%",
    "78 - Transition Ramp Down Medium Sharp 1 - 100 to 0%",
    "79 - Transition Ramp Down Medium Sharp 2 - 100 to 0%",
    "80 - Transition Ramp Down Short Sharp 1 - 100 to 0%",
    "81 - Transition Ramp Down Short Sharp 2 - 100 to 0%",
    "82 - Transition Ramp Up Long Smooth 1 - 0 to 100%",
    "83 - Transition Ramp Up Long Smooth 2 - 0 to 100%",
    "84 - Transition Ramp Up Medium Smooth 1 - 0 to 100%",
    "85 - Transition Ramp Up Medium Smooth 2 - 0 to 100%",
    "86 - Transition Ramp Up Short Smooth 1 - 0 to 100%",
    "87 - Transition Ramp Up Short Smooth 2 - 0 to 100%",
    "88 - Transition Ramp Up Long Sharp 1 - 0 to 100%",
    "89 - Transition Ramp Up Long Sharp 2 - 0 to 100%",
    "90 - Transition Ramp Up Medium Sharp 1 - 0 to 100%",
    "91 - Transition Ramp Up Medium Sharp 2 - 0 to 100%",
    "92 - Transition Ramp Up Short Sharp 1 - 0 to 100%",
    "93 - Transition Ramp Up Short Sharp 2 - 0 to 100%",
    "94 - Transition Ramp Down Long Smooth 1 - 50 to 0%",
    "95 - Transition Ramp Down Long Smooth 2 - 50 to 0%",
    "96 - Transition Ramp Down Medium Smooth 1 - 50 to 0%",
    "97 - Transition Ramp Down Medium Smooth 2 - 50 to 0%",
    "98 - Transition Ramp Down Short Smooth 1 - 50 to 0%",
    "99 - Transition Ramp Down Short Smooth 2 - 50 to 0%",
    "100 - Transition Ramp Down Long Sharp 1 - 50 to 0%",
    "101 - Transition Ramp Down Long Sharp 2 - 50 to 0%",
    "102 - Transition Ramp Down Medium Sharp 1 - 50 to 0%",
    "103 - Transition Ramp Down Medium Sharp 2 - 50 to 0%",
    "104 - Transition Ramp Down Short Sharp 1 - 50 to 0%",
    "105 - Transition Ramp Down Short Sharp 2 - 50 to 0%",
    "106 - Transition Ramp Up Long Smooth 1 - 0 to 50%",
    "107 - Transition Ramp Up Long Smooth 2 - 0 to 50%",
    "108 - Transition Ramp Up Medium Smooth 1 - 0 to 50%",
    "109 - Transition Ramp Up Medium Smooth 2 - 0 to 50%",
    "110 - Transition Ramp Up Short Smooth 1 - 0 to 50%",
    "111 - Transition Ramp Up Short Smooth 2 - 0 to 50%",
    "112 - Transition Ramp Up Long Sharp 1 - 0 to 50%",
    "113 - Transition Ramp Up Long Sharp 2 - 0 to 50%",
    "114 - Transition Ramp Up Medium Sharp 1 - 0 to 50%",
    "115 - Transition Ramp Up Medium Sharp 2 - 0 to 50%",
    "116 - Transition Ramp Up Short Sharp 1 - 0 to 50%",
    "117 - Transition Ramp Up Short Sharp 2 - 0 to 50%",
    "118 - Long buzz for programmatic stopping - 100%",
    "119 - Smooth Hum 1 (No kick or brake pulse) - 50%",
    "120 - Smooth Hum 2 (No kick or brake pulse) - 40%",
    "121 - Smooth Hum 3 (No kick or brake pulse) - 30%",
    "122 - Smooth Hum 4 (No kick or brake pulse) - 20%",
    "123 - Smooth Hum 5 (No kick or brake pulse) - 10%"};

void print_effect_description(int effect)
{
    // if (effect == 1)
    // {
    //     ESP_LOGI(TAG, "11.2 Waveform Library Effects List");
    // }

    if (effect >= 1 && effect <= MAX_EFFECT_ID)
    {
        ESP_LOGI(TAG, "Effect #%s", effect_descriptions[effect]);
    }
    else
    {
        ESP_LOGI(TAG, "Unknown effect: %d", effect);
    }
}

esp_err_t drv2605_init()
{
    ESP_LOGI(TAG, "----DRIVER DRV2605----");

    //* Implemented using read and write callback methods, applicable to other platforms
#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

    // * Provide the device address to the callback function
    i2c_drv_device_init(DRV2605_I2C_ADDRESS);

    ESP_LOGI(TAG, "Implemented using read and write callback methods");
    if (drv.begin(i2c_wr_function, hal_callback, DRV2605_I2C_ADDRESS))
    {
        ESP_LOGI(TAG, "Initialization of DRV2605 haptic driver is successful!");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize the DRV2605 haptic driver!");
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

    if (drv.begin(bus_handle))
    {
        ESP_LOGI(TAG, "Initialization of DRV2605 haptic driver is successful!");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize the DRV2605 haptic driver!");
        return ESP_FAIL;
    }

#else

    ESP_LOGI(TAG, "Implemented using built-in read and write methods (Use lower version < 5.0 API)");
    if (drv.begin((i2c_port_t)CONFIG_I2C_MASTER_PORT_NUM, CONFIG_SENSOR_SDA, CONFIG_SENSOR_SCL))
    {
        ESP_LOGI(TAG, "Initialization of DRV2605 haptic driver is successful!");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize the DRV2605 haptic driver!");
        return ESP_FAIL;
    }
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
#endif // CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW

    // Use LRA (Linear Resonance Actuator) mode.
    drv.useLRA();
    // drv.useERM();

    // Select the waveform Library to use
    // 0 = Empty, 1-5 are ERM, 6 is LRA.
    drv.selectLibrary(6);

    // I2C trigger by sending 'run' command
    // default, internal trigger when sending RUN command
    drv.setMode(SensorDRV2605::MODE_INTTRIG);

    init_done = true;

    return ESP_OK;
}

void drv2605_loop()
{
    if (!init_done)
    {
        return;
    }

    print_effect_description(effect);

    // set the effect to play
    drv.setWaveform(0, effect); // play effect
    drv.setWaveform(1, 0);      // end waveform

    // play the effect!
    drv.run();

    // wait a bit
    vTaskDelay(pdMS_TO_TICKS(1000));

    effect++;

    if (effect == 118)
    {
        effect = 119;
        ESP_LOGW(TAG, "118 is a special effect for programmatic stopping, not a real effect.");
    }

    if (effect > MAX_EFFECT_ID)
    {
        effect = 1;
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
