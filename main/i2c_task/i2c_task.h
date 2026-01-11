#pragma once

// #include "i2c_app.h"

#include "sensor_bm8563.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

    // #include "i2c_port.h"

    void i2c_peripheral_init_task(void *pvParameters);

#ifdef __cplusplus
}
#endif