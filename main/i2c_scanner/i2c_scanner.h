#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/i2c.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"

#define I2C_MASTER_NUM (i2c_port_t) CONFIG_I2C_MASTER_PORT_NUM
#define I2C_SDA_PIN (gpio_num_t) CONFIG_PMU_I2C_SDA
#define I2C_SCL_PIN (gpio_num_t) CONFIG_PMU_I2C_SCL
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQUENCY

#ifdef __cplusplus
extern "C"
{
#endif

    void i2c_scanner_task(void *ignore);

#ifdef __cplusplus
}
#endif
