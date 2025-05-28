#pragma once

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_XPOWERS_ESP_IDF_NEW_API)
#include "driver/i2c_master.h"
#else
#include "driver/i2c.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);
    int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);
    esp_err_t i2c_init(void);

#ifdef __cplusplus
}
#endif
