#include "i2c_driver.h"
#include "axp202_port.h"
#include "hp28008.h"

#if defined(CONFIG_XPOWERS_ESP_IDF_NEW_API) && !defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
#include "i2c_port.h"
#endif

static const char *TAG = "MAIN-LGVL";

void app_main(void)
{
#if CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW || CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW

#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW || \
    ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && \
     (defined(CONFIG_XPOWERS_ESP_IDF_NEW_API) || defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)))
#if defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API)
    ESP_ERROR_CHECK(i2c_drv_init());
#elif defined(CONFIG_XPOWERS_ESP_IDF_NEW_API)
    ESP_ERROR_CHECK(i2c_init());
#endif
#endif

    ESP_LOGI(TAG, "I2C initialized successfully");

    ESP_ERROR_CHECK(axp202_init());

    // Run bus scan
    i2c_drv_scan();

    ESP_ERROR_CHECK(bm8563_init());

#endif

    /* LCD HW initialization */
    ESP_ERROR_CHECK(app_lcd_init());

    /* Touch initialization */
    ESP_ERROR_CHECK(app_touch_init());

    /* LVGL initialization */
    ESP_ERROR_CHECK(app_lvgl_init());

    /* Show LVGL objects */
    app_main_display();
}
