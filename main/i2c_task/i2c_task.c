#include "i2c_task.h"

static const char *TAG = "I2C peripheral";

void i2c_peripheral_init_task(void *pvParameters)
{
    ESP_LOGI(TAG, "i2C peripheral task");

    // rtc_bm8563_init();
    bm8563_init();

    vTaskDelete(NULL);
}