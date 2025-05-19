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

    esp_err_t pmu_init();
    void printPMU();
    void enterPmuSleep(void);
    void pmu_isr_handler();

#ifdef __cplusplus
}
#endif
