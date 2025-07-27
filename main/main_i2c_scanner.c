#include "i2c_scanner.h"

static const char *TAG = "MAIN-I2C_SCANNER";

void app_main()
{
#if CONFIG_FREERTOS_UNICORE
    xTaskCreate(i2c_scanner_task, TAG, configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL);
#else
    xTaskCreatePinnedToCore(i2c_scanner_task, TAG, configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM);
#endif
}
