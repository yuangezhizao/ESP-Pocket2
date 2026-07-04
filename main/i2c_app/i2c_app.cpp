#include "i2c_app.h"

static const char *TAG = "I2C_APP";

struct tm timeinfo;

char RTC_global_variable[32];

bool update_RTC_global_variable(void)
{
    rtc.getDateTime(&timeinfo);

    // Format the output using the strftime function
    // For more formats, please refer to :
    // https://man7.org/linux/man-pages/man3/strftime.3.html
    // size_t written = strftime(RTC_global_variable, 32, "%Y-%m-%d %H:%M:%S %Z(%z)", &timeinfo);
    size_t written = strftime(RTC_global_variable, sizeof(RTC_global_variable), "%Y-%m-%d %H:%M:%S %Z(%z)", &timeinfo);

    if (written != 0)
    {
        ESP_LOGI(TAG, "%s", RTC_global_variable);
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "strftime failed or buffer too small!");
        // 失败时清空缓冲，避免调用方读到上一次的旧时间
        RTC_global_variable[0] = '\0';
        return false;
    }
}
