#pragma once

#include "esp_log.h"
#include "esp_err.h"

#include "sensor_bm8563.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void update_RTC_global_variable(void);

#ifdef __cplusplus
}
#endif

extern char RTC_global_variable[32];
