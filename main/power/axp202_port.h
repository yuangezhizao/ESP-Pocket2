#pragma once

#include "i2c_port.h"

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t axp202_init();
    void axp202_show_info();
    void axp202_enter_sleep();
    void axp202_isr_handler();

#ifdef __cplusplus
}
#endif
