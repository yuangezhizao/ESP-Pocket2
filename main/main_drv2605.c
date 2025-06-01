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
 * @file      main_drv2605.c
 * @author    Lewis He (lewishe@outlook.com)
 * @date      2025-06-01
 *
 */
#include "sdkconfig.h"

// #include "i2c_driver.h"
#include "axp202_port.h"
#include "sensor_drv2605.h"

static const char *TAG = "MAIN-DRV2605";

static void app_task(void *args);

// extern "C" void app_main(void)
void app_main(void)
{
    ESP_ERROR_CHECK(pmu_init());

#if CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW || CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

    // ESP_ERROR_CHECK(i2c_drv_init());

    ESP_LOGI(TAG, "I2C initialized successfully");

    // Run bus scan
    i2c_drv_scan();

#endif

    ESP_ERROR_CHECK(drv2605_init());

    ESP_LOGI(TAG, "Run...");

    xTaskCreate(app_task, "App", 20 * 1024, NULL, 10, NULL);
}

static void app_task(void *args)
{
    while (1)
    {
        drv2605_loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
