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

#include "HapticDrivers.hpp"

static const char *TAG = "DRV2605";

#define DRV2605_I2C_ADDRESS 0x5A
#define MAX_EFFECT_ID 123
#define DRV2605_WAVEFORM_LIBRARY 6

/** TI DRV2605 ROM waveform library effect metadata (ID 1-123). */
struct Drv2605EffectInfo
{
    const char *en;
    const char *zh;
};

/** Nine effect categories for grouped log output. */
struct Drv2605EffectCategory
{
    uint8_t first_id;
    uint8_t last_id;
    const char *name_en;
    const char *name_zh;
};

#define DRV2605_EFFECT_CATEGORY_COUNT (sizeof(effect_categories) / sizeof(effect_categories[0]))

static const Drv2605EffectCategory effect_categories[] = {
    {1, 16, "Basic Clicks", "基础点击类"},
    {17, 46, "Graded Clicks & Ticks", "分级点击/滴答类"},
    {47, 57, "Buzz & Pulsing", "嗡嗡声与脉冲类"},
    {58, 69, "Transition Click & Hum", "过渡点击/嗡鸣类"},
    {70, 81, "Ramp Down 100% to 0%", "过渡渐弱 100%→0%"},
    {82, 93, "Ramp Up 0% to 100%", "过渡渐强 0%→100%"},
    {94, 105, "Ramp Down 50% to 0%", "过渡渐弱 50%→0%"},
    {106, 117, "Ramp Up 0% to 50%", "过渡渐强 0%→50%"},
    {118, 123, "Special Effects", "特殊效果"},
};

static const Drv2605EffectInfo effect_info[MAX_EFFECT_ID + 1] = {
    {"", ""},
    {"Strong Click - 100%", "强点击 - 100%"},
    {"Strong Click - 60%", "强点击 - 60%"},
    {"Strong Click - 30%", "强点击 - 30%"},
    {"Sharp Click - 100%", "锐点击 - 100%"},
    {"Sharp Click - 60%", "锐点击 - 60%"},
    {"Sharp Click - 30%", "锐点击 - 30%"},
    {"Soft Bump - 100%", "软碰撞 - 100%"},
    {"Soft Bump - 60%", "软碰撞 - 60%"},
    {"Soft Bump - 30%", "软碰撞 - 30%"},
    {"Double Click - 100%", "双点击 - 100%"},
    {"Double Click - 60%", "双点击 - 60%"},
    {"Triple Click - 100%", "三点击 - 100%"},
    {"Soft Fuzz - 60%", "柔和嗡鸣 - 60%"},
    {"Strong Buzz - 100%", "强嗡嗡声 - 100%"},
    {"750 ms Alert - 100%", "750 毫秒警报 - 100%"},
    {"1000 ms Alert - 100%", "1000 毫秒警报 - 100%"},
    {"Strong Click 1 - 100%", "强点击 1 - 100%"},
    {"Strong Click 2 - 80%", "强点击 2 - 80%"},
    {"Strong Click 3 - 60%", "强点击 3 - 60%"},
    {"Strong Click 4 - 30%", "强点击 4 - 30%"},
    {"Medium Click 1 - 100%", "中等点击 1 - 100%"},
    {"Medium Click 2 - 80%", "中等点击 2 - 80%"},
    {"Medium Click 3 - 60%", "中等点击 3 - 60%"},
    {"Sharp Tick 1 - 100%", "锐滴答 1 - 100%"},
    {"Sharp Tick 2 - 80%", "锐滴答 2 - 80%"},
    {"Sharp Tick 3 - 60%", "锐滴答 3 - 60%"},
    {"Short Double Click Strong 1 - 100%", "短双点击（强）1 - 100%"},
    {"Short Double Click Strong 2 - 80%", "短双点击（强）2 - 80%"},
    {"Short Double Click Strong 3 - 60%", "短双点击（强）3 - 60%"},
    {"Short Double Click Strong 4 - 30%", "短双点击（强）4 - 30%"},
    {"Short Double Click Medium 1 - 100%", "短双点击（中）1 - 100%"},
    {"Short Double Click Medium 2 - 80%", "短双点击（中）2 - 80%"},
    {"Short Double Click Medium 3 - 60%", "短双点击（中）3 - 60%"},
    {"Short Double Sharp Tick 1 - 100%", "短双锐滴答 1 - 100%"},
    {"Short Double Sharp Tick 2 - 80%", "短双锐滴答 2 - 80%"},
    {"Short Double Sharp Tick 3 - 60%", "短双锐滴答 3 - 60%"},
    {"Long Double Sharp Click Strong 1 - 100%", "长双锐点击（强）1 - 100%"},
    {"Long Double Sharp Click Strong 2 - 80%", "长双锐点击（强）2 - 80%"},
    {"Long Double Sharp Click Strong 3 - 60%", "长双锐点击（强）3 - 60%"},
    {"Long Double Sharp Click Strong 4 - 30%", "长双锐点击（强）4 - 30%"},
    {"Long Double Sharp Click Medium 1 - 100%", "长双锐点击（中）1 - 100%"},
    {"Long Double Sharp Click Medium 2 - 80%", "长双锐点击（中）2 - 80%"},
    {"Long Double Sharp Click Medium 3 - 60%", "长双锐点击（中）3 - 60%"},
    {"Long Double Sharp Tick 1 - 100%", "长双锐滴答 1 - 100%"},
    {"Long Double Sharp Tick 2 - 80%", "长双锐滴答 2 - 80%"},
    {"Long Double Sharp Tick 3 - 60%", "长双锐滴答 3 - 60%"},
    {"Buzz 1 - 100%", "嗡嗡声 1 - 100%"},
    {"Buzz 2 - 80%", "嗡嗡声 2 - 80%"},
    {"Buzz 3 - 60%", "嗡嗡声 3 - 60%"},
    {"Buzz 4 - 40%", "嗡嗡声 4 - 40%"},
    {"Buzz 5 - 20%", "嗡嗡声 5 - 20%"},
    {"Pulsing Strong 1 - 100%", "脉冲（强）1 - 100%"},
    {"Pulsing Strong 2 - 60%", "脉冲（强）2 - 60%"},
    {"Pulsing Medium 1 - 100%", "脉冲（中）1 - 100%"},
    {"Pulsing Medium 2 - 60%", "脉冲（中）2 - 60%"},
    {"Pulsing Sharp 1 - 100%", "脉冲（锐）1 - 100%"},
    {"Pulsing Sharp 2 - 60%", "脉冲（锐）2 - 60%"},
    {"Transition Click 1 - 100%", "过渡点击 1 - 100%"},
    {"Transition Click 2 - 80%", "过渡点击 2 - 80%"},
    {"Transition Click 3 - 60%", "过渡点击 3 - 60%"},
    {"Transition Click 4 - 40%", "过渡点击 4 - 40%"},
    {"Transition Click 5 - 20%", "过渡点击 5 - 20%"},
    {"Transition Click 6 - 10%", "过渡点击 6 - 10%"},
    {"Transition Hum 1 - 100%", "过渡嗡鸣 1 - 100%"},
    {"Transition Hum 2 - 80%", "过渡嗡鸣 2 - 80%"},
    {"Transition Hum 3 - 60%", "过渡嗡鸣 3 - 60%"},
    {"Transition Hum 4 - 40%", "过渡嗡鸣 4 - 40%"},
    {"Transition Hum 5 - 20%", "过渡嗡鸣 5 - 20%"},
    {"Transition Hum 6 - 10%", "过渡嗡鸣 6 - 10%"},
    {"Transition Ramp Down Long Smooth 1 - 100 to 0%", "过渡渐弱 长 平滑 1 - 100%→0%"},
    {"Transition Ramp Down Long Smooth 2 - 100 to 0%", "过渡渐弱 长 平滑 2 - 100%→0%"},
    {"Transition Ramp Down Medium Smooth 1 - 100 to 0%", "过渡渐弱 中 平滑 1 - 100%→0%"},
    {"Transition Ramp Down Medium Smooth 2 - 100 to 0%", "过渡渐弱 中 平滑 2 - 100%→0%"},
    {"Transition Ramp Down Short Smooth 1 - 100 to 0%", "过渡渐弱 短 平滑 1 - 100%→0%"},
    {"Transition Ramp Down Short Smooth 2 - 100 to 0%", "过渡渐弱 短 平滑 2 - 100%→0%"},
    {"Transition Ramp Down Long Sharp 1 - 100 to 0%", "过渡渐弱 长 尖锐 1 - 100%→0%"},
    {"Transition Ramp Down Long Sharp 2 - 100 to 0%", "过渡渐弱 长 尖锐 2 - 100%→0%"},
    {"Transition Ramp Down Medium Sharp 1 - 100 to 0%", "过渡渐弱 中 尖锐 1 - 100%→0%"},
    {"Transition Ramp Down Medium Sharp 2 - 100 to 0%", "过渡渐弱 中 尖锐 2 - 100%→0%"},
    {"Transition Ramp Down Short Sharp 1 - 100 to 0%", "过渡渐弱 短 尖锐 1 - 100%→0%"},
    {"Transition Ramp Down Short Sharp 2 - 100 to 0%", "过渡渐弱 短 尖锐 2 - 100%→0%"},
    {"Transition Ramp Up Long Smooth 1 - 0 to 100%", "过渡渐强 长 平滑 1 - 0%→100%"},
    {"Transition Ramp Up Long Smooth 2 - 0 to 100%", "过渡渐强 长 平滑 2 - 0%→100%"},
    {"Transition Ramp Up Medium Smooth 1 - 0 to 100%", "过渡渐强 中 平滑 1 - 0%→100%"},
    {"Transition Ramp Up Medium Smooth 2 - 0 to 100%", "过渡渐强 中 平滑 2 - 0%→100%"},
    {"Transition Ramp Up Short Smooth 1 - 0 to 100%", "过渡渐强 短 平滑 1 - 0%→100%"},
    {"Transition Ramp Up Short Smooth 2 - 0 to 100%", "过渡渐强 短 平滑 2 - 0%→100%"},
    {"Transition Ramp Up Long Sharp 1 - 0 to 100%", "过渡渐强 长 尖锐 1 - 0%→100%"},
    {"Transition Ramp Up Long Sharp 2 - 0 to 100%", "过渡渐强 长 尖锐 2 - 0%→100%"},
    {"Transition Ramp Up Medium Sharp 1 - 0 to 100%", "过渡渐强 中 尖锐 1 - 0%→100%"},
    {"Transition Ramp Up Medium Sharp 2 - 0 to 100%", "过渡渐强 中 尖锐 2 - 0%→100%"},
    {"Transition Ramp Up Short Sharp 1 - 0 to 100%", "过渡渐强 短 尖锐 1 - 0%→100%"},
    {"Transition Ramp Up Short Sharp 2 - 0 to 100%", "过渡渐强 短 尖锐 2 - 0%→100%"},
    {"Transition Ramp Down Long Smooth 1 - 50 to 0%", "过渡渐弱 长 平滑 1 - 50%→0%"},
    {"Transition Ramp Down Long Smooth 2 - 50 to 0%", "过渡渐弱 长 平滑 2 - 50%→0%"},
    {"Transition Ramp Down Medium Smooth 1 - 50 to 0%", "过渡渐弱 中 平滑 1 - 50%→0%"},
    {"Transition Ramp Down Medium Smooth 2 - 50 to 0%", "过渡渐弱 中 平滑 2 - 50%→0%"},
    {"Transition Ramp Down Short Smooth 1 - 50 to 0%", "过渡渐弱 短 平滑 1 - 50%→0%"},
    {"Transition Ramp Down Short Smooth 2 - 50 to 0%", "过渡渐弱 短 平滑 2 - 50%→0%"},
    {"Transition Ramp Down Long Sharp 1 - 50 to 0%", "过渡渐弱 长 尖锐 1 - 50%→0%"},
    {"Transition Ramp Down Long Sharp 2 - 50 to 0%", "过渡渐弱 长 尖锐 2 - 50%→0%"},
    {"Transition Ramp Down Medium Sharp 1 - 50 to 0%", "过渡渐弱 中 尖锐 1 - 50%→0%"},
    {"Transition Ramp Down Medium Sharp 2 - 50 to 0%", "过渡渐弱 中 尖锐 2 - 50%→0%"},
    {"Transition Ramp Down Short Sharp 1 - 50 to 0%", "过渡渐弱 短 尖锐 1 - 50%→0%"},
    {"Transition Ramp Down Short Sharp 2 - 50 to 0%", "过渡渐弱 短 尖锐 2 - 50%→0%"},
    {"Transition Ramp Up Long Smooth 1 - 0 to 50%", "过渡渐强 长 平滑 1 - 0%→50%"},
    {"Transition Ramp Up Long Smooth 2 - 0 to 50%", "过渡渐强 长 平滑 2 - 0%→50%"},
    {"Transition Ramp Up Medium Smooth 1 - 0 to 50%", "过渡渐强 中 平滑 1 - 0%→50%"},
    {"Transition Ramp Up Medium Smooth 2 - 0 to 50%", "过渡渐强 中 平滑 2 - 0%→50%"},
    {"Transition Ramp Up Short Smooth 1 - 0 to 50%", "过渡渐强 短 平滑 1 - 0%→50%"},
    {"Transition Ramp Up Short Smooth 2 - 0 to 50%", "过渡渐强 短 平滑 2 - 0%→50%"},
    {"Transition Ramp Up Long Sharp 1 - 0 to 50%", "过渡渐强 长 尖锐 1 - 0%→50%"},
    {"Transition Ramp Up Long Sharp 2 - 0 to 50%", "过渡渐强 长 尖锐 2 - 0%→50%"},
    {"Transition Ramp Up Medium Sharp 1 - 0 to 50%", "过渡渐强 中 尖锐 1 - 0%→50%"},
    {"Transition Ramp Up Medium Sharp 2 - 0 to 50%", "过渡渐强 中 尖锐 2 - 0%→50%"},
    {"Transition Ramp Up Short Sharp 1 - 0 to 50%", "过渡渐强 短 尖锐 1 - 0%→50%"},
    {"Transition Ramp Up Short Sharp 2 - 0 to 50%", "过渡渐强 短 尖锐 2 - 0%→50%"},
    {"Long buzz for programmatic stopping - 100%", "程序控制停止用长嗡鸣 - 100% [非演示效果]"},
    {"Smooth Hum 1 (No kick or brake pulse) - 50%", "平滑嗡鸣 1（无启动/制动脉冲）- 50%"},
    {"Smooth Hum 2 (No kick or brake pulse) - 40%", "平滑嗡鸣 2（无启动/制动脉冲）- 40%"},
    {"Smooth Hum 3 (No kick or brake pulse) - 30%", "平滑嗡鸣 3（无启动/制动脉冲）- 30%"},
    {"Smooth Hum 4 (No kick or brake pulse) - 20%", "平滑嗡鸣 4（无启动/制动脉冲）- 20%"},
    {"Smooth Hum 5 (No kick or brake pulse) - 10%", "平滑嗡鸣 5（无启动/制动脉冲）- 10%"},
};

HapticDriver_DRV2605 drv;

// extern uint32_t hal_callback(SensorCommCustomHal::Operation op, void *param1, void *param2);

static bool init_done = false;
static int last_logged_category = -1;

uint8_t effect = 1;

static int drv2605_find_category_index(uint8_t effect_id)
{
    for (size_t i = 0; i < DRV2605_EFFECT_CATEGORY_COUNT; ++i)
    {
        if (effect_id >= effect_categories[i].first_id && effect_id <= effect_categories[i].last_id)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void print_effect_description(int effect)
{
    if (effect == 1)
    {
        last_logged_category = -1;
    }

    if (effect >= 1 && effect <= MAX_EFFECT_ID)
    {
        const int category_index = drv2605_find_category_index(static_cast<uint8_t>(effect));
        if (category_index >= 0 && category_index != last_logged_category)
        {
            const Drv2605EffectCategory &category = effect_categories[category_index];
            ESP_LOGI(TAG, "── [%d/%d] %s / %s · ID %u-%u ──",
                     category_index + 1,
                     static_cast<int>(DRV2605_EFFECT_CATEGORY_COUNT),
                     category.name_zh,
                     category.name_en,
                     category.first_id,
                     category.last_id);
            last_logged_category = category_index;
        }

        ESP_LOGI(TAG, "#%03u  %s  |  %s",
                 effect,
                 effect_info[effect].en,
                 effect_info[effect].zh);
    }
    else
    {
        ESP_LOGI(TAG, "Unknown effect #%d / 未知效果 #%d", effect, effect);
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
    if (drv.begin((i2c_port_t)CONFIG_I2C_MASTER_PORT_NUM, DRV2605_I2C_ADDRESS, -1, -1))
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
    drv.setActuatorType(HapticActuatorType::LRA);
    // drv.setActuatorType(HapticActuatorType::ERM);

    // Select the waveform Library to use
    // 0 = Empty, 1-5 are ERM, 6 is LRA.
    drv.selectLibrary(DRV2605_WAVEFORM_LIBRARY);

    const HapticActuatorType actuator = drv.getActuatorType();
    ESP_LOGI(TAG, "Actuator: %s / %s  |  Library: %u",
             actuator == HapticActuatorType::LRA ? "LRA" : "ERM",
             actuator == HapticActuatorType::LRA ? "线性谐振马达" : "偏心旋转马达",
             DRV2605_WAVEFORM_LIBRARY);

    ESP_LOGI(TAG, "Auto-calibrating... / 正在自动校准...");
    const int cal_result = drv.autoCal();
    if (cal_result >= 0)
    {
        ESP_LOGI(TAG, "Auto-cal OK, comp=%d / 自动校准成功，补偿值=%d", cal_result, cal_result);
        const uint32_t f0_hz = drv.getF0();
        if (f0_hz > 0)
        {
            ESP_LOGI(TAG, "LRA F0: %lu Hz / 谐振频率: %lu Hz", (unsigned long)f0_hz, (unsigned long)f0_hz);
        }
    }
    else
    {
        ESP_LOGW(TAG, "Auto-cal failed, using defaults / 自动校准失败，使用默认参数");
    }

    // I2C trigger by sending 'run' command
    // default, internal trigger when sending RUN command
    drv.setMode(HapticMode::INTERNAL_TRIGGER);

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
        ESP_LOGW(TAG, "Skip #118 (programmatic stop only) | 跳过 #118（仅程序控制停止）");
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
