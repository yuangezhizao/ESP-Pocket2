#pragma once

#include <stdbool.h>

#include "esp_log.h"
#include "esp_err.h"

#include "sensor_bm8563.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* 返回 true=成功刷新 RTC_global_variable；false=strftime 格式化失败（此时缓冲已清为空串）。注意：不检测 rtc.getDateTime() I2C 读失败——读失败但 timeinfo 仍为旧值时 strftime 仍成功、依旧返回 true（见 docs/superpowers 07-04 的 PR6-1 已知边界）。 */
    bool update_RTC_global_variable(void);

#ifdef __cplusplus
}
#endif

extern char RTC_global_variable[32];
