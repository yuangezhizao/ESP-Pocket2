/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hp28008.h"

#define BASE_TAG "HP28008"

static const char *TAG = BASE_TAG;
static const char *TAG_LCD = BASE_TAG "-LCD";
static const char *TAG_TOUCH = BASE_TAG "-TOUCH";
static const char *TAG_LVGL = BASE_TAG "-LVGL";

// LVGL image declare
LV_IMG_DECLARE(esp_logo)

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

#if EXAMPLE_LCD_BL_USE_LEDC == (1)
static void example_ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY, // Set output frequency at 4 kHz
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO,
        .duty = 0, // Set duty to 0%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void example_ledc_set_lcd_brightness(uint32_t percentage)
{
    ESP_LOGI(TAG_LCD, "Set LCD BL brightness to %" PRIu32 "%%", percentage);

    // Check if the percentage is valid
    if (percentage > 100)
    {
        ESP_LOGE(TAG_LCD, "BL brightness percentage %" PRIu32 "%% is out of range (0-100), resetting to default 50%%", percentage);
        percentage = 50;
    }

    // Convert percentage to duty
    uint32_t duty = (percentage / 100.00) * LEDC_DUTY_MAX;

    // Set duty to ledc_duty
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}
#endif

// static esp_err_t app_lcd_init(void)
esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    /* LCD backlight */
    ESP_LOGI(TAG_LCD, "[1/5] Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_LCD_GPIO_BL};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    /* LCD initialization */
    ESP_LOGI(TAG_LCD, "[2/5] Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_LCD_GPIO_SCLK,
        .mosi_io_num = EXAMPLE_LCD_GPIO_MOSI,
        .miso_io_num = EXAMPLE_LCD_GPIO_MISO, // Note: GPIO_NUM_NC
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t), // Note: EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t)};
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(EXAMPLE_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGI(TAG_LCD, "[3/5] Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_LCD_GPIO_DC,
        .cs_gpio_num = EXAMPLE_LCD_GPIO_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    // Attach the LCD to the SPI bus
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_SPI_NUM, &io_config, &lcd_io), err, TAG, "New panel IO failed");

    ESP_LOGI(TAG_LCD, "[4/5] Install ST7789 panel driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_LCD_GPIO_RST,
        .color_space = EXAMPLE_LCD_COLOR_SPACE,
        .bits_per_pixel = EXAMPLE_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel), err, TAG, "New panel failed");

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
#if EXAMPLE_LCD_INVERT_COLOR == (1)
    esp_lcd_panel_invert_color(lcd_panel, EXAMPLE_LCD_INVERT_COLOR);
#endif
    // esp_lcd_panel_swap_xy(lcd_panel, EXAMPLE_LCD_DIRECTION_SWAP_X_Y);                                // Note: DO NOT NEED
    // esp_lcd_panel_mirror(lcd_panel, EXAMPLE_LCD_DIRECTION_MIRROR_X, EXAMPLE_LCD_DIRECTION_MIRROR_Y); // Note: DO NOT NEED

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    esp_lcd_panel_disp_on_off(lcd_panel, true);

    /* LCD backlight on */
#if EXAMPLE_LCD_BL_USE_LEDC == (1)
    ESP_LOGI(TAG_LCD, "[5/5] Turn on LCD backlight by LEDC");

    // Set the LEDC peripheral configuration
    example_ledc_init();

    // Set the duty cycle for the LEDC channel
    example_ledc_set_lcd_brightness(50);
#else
    ESP_LOGI(TAG_LCD, "[5/5] Turn on LCD backlight by GPIO");
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_LCD_GPIO_BL, EXAMPLE_LCD_BL_ON_LEVEL));
#endif

    return ret;

err:
    if (lcd_panel)
    {
        esp_lcd_panel_del(lcd_panel);
    }
    if (lcd_io)
    {
        esp_lcd_panel_io_del(lcd_io);
    }
    spi_bus_free(EXAMPLE_LCD_SPI_NUM);
    return ret;
}

// static esp_err_t app_touch_init(void)
esp_err_t app_touch_init(void)
{
    /* Initilize I2C */
    ESP_LOGI(TAG_TOUCH, "[1/2] Initialize I2C bus");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_TOUCH_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE, // Note: GPIO_PULLUP_ENABLE
        .scl_io_num = EXAMPLE_TOUCH_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE, // Note: GPIO_PULLUP_ENABLE
        .master.clk_speed = EXAMPLE_TOUCH_I2C_CLK_HZ};
    ESP_RETURN_ON_ERROR(i2c_param_config(EXAMPLE_TOUCH_I2C_NUM, &i2c_conf), TAG, "I2C configuration failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(EXAMPLE_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0), TAG, "I2C initialization failed");

    /* Initialize touch HW */
    ESP_LOGI(TAG_TOUCH, "[2/2] Initialize touch controller GT911");
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_TOUCH_GPIO_RST, // Note: If shared with LCD reset, use GPIO_NUM_NC
        .int_gpio_num = EXAMPLE_TOUCH_GPIO_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        // .flags = {
        //     .swap_xy = EXAMPLE_LCD_DIRECTION_SWAP_X_Y,
        //     .mirror_x = EXAMPLE_LCD_DIRECTION_MIRROR_X,
        //     .mirror_y = EXAMPLE_LCD_DIRECTION_MIRROR_Y,
        // }, // Note: DO NOT NEED
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

    // Attach the TOUCH to the I2C bus
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &touch_handle);
}

// static esp_err_t app_lvgl_init(void)
esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    ESP_LOGI(TAG_LVGL, "[1/3] Initialize LVGL library");
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = EXAMPLE_LVGL_TASK_PRIORITY,         /* LVGL task priority */
        .task_stack = EXAMPLE_LVGL_TASK_STACK_SIZE,          /* LVGL task stack size */
        .task_affinity = EXAMPLE_LVGL_TASK_AFFINITY,         /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS, /* Maximum sleep in LVGL task */
        .timer_period_ms = EXAMPLE_LVGL_TICK_PERIOD_MS       /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGI(TAG_LVGL, "[2/3] Add LCD screen for LVGL");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        // .rotation = {
        //     .swap_xy = EXAMPLE_LCD_DIRECTION_SWAP_X_Y,
        //     .mirror_x = EXAMPLE_LCD_DIRECTION_MIRROR_X,
        //     .mirror_y = EXAMPLE_LCD_DIRECTION_MIRROR_Y,
        // }, // Note: DO NOT NEED
        .flags = {
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        }};
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Add touch input (for selected screen) */
    ESP_LOGI(TAG_LVGL, "[3/3] Add touch input for LVGL");
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}

static void _app_button_cb(lv_event_t *e)
{
    lv_disp_rotation_t rotation = lv_disp_get_rotation(lvgl_disp);
    rotation++;
    if (rotation > LV_DISPLAY_ROTATION_270)
    {
        rotation = LV_DISPLAY_ROTATION_0;
    }

    /* LCD HW rotation */
    lv_disp_set_rotation(lvgl_disp, rotation);
}

// static void app_main_display(void)
void app_main_display(void)
{
    ESP_LOGI(TAG, "Display LVGL example");

#if EXAMPLE_LCD_DIRECTION == (90)
    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_90);
#elif EXAMPLE_LCD_DIRECTION == (180)
    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_180);
#elif EXAMPLE_LCD_DIRECTION == (270)
    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_270);
#endif

    lv_obj_t *scr = lv_scr_act();

    /* Task lock */
    lvgl_port_lock(0);

    /* Your LVGL objects code here .... */
    // Lock the mutex due to the LVGL APIs are not thread-safe

    /* Create image */
    lv_obj_t *img_logo = lv_img_create(scr);
    lv_img_set_src(img_logo, &esp_logo);
    lv_obj_align(img_logo, LV_ALIGN_TOP_MID, 0, 20);

    /* Label */
    lv_obj_t *label = lv_label_create(scr);
    lv_obj_set_width(label, EXAMPLE_LCD_H_RES);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
#if LVGL_VERSION_MAJOR == 8
    lv_label_set_recolor(label, true);
    lv_label_set_text(label, "#FF0000 " LV_SYMBOL_BELL " Hello world Espressif and LVGL " LV_SYMBOL_BELL "#\n#FF9400 " LV_SYMBOL_WARNING " For simplier initialization, use BSP " LV_SYMBOL_WARNING " #");
#else
    lv_label_set_text(label, LV_SYMBOL_BELL " Hello world Espressif and LVGL " LV_SYMBOL_BELL "\n " LV_SYMBOL_WARNING " For simplier initialization, use BSP " LV_SYMBOL_WARNING);
#endif
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);

    /* Button */
    lv_obj_t *btn = lv_btn_create(scr);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, "Rotate screen");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(btn, _app_button_cb, LV_EVENT_CLICKED, NULL);

    /* Task unlock */
    lvgl_port_unlock();
}
