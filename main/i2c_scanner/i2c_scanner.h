#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/i2c.h>

#include "esp_log.h"
#include "esp_err.h"

#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define I2C_MASTER_FREQ_HZ 100000

#ifdef __cplusplus
extern "C"
{
#endif

    void i2c_scanner_task(void *ignore);

#ifdef __cplusplus
}
#endif
