#include "axp202_port.h"

#ifdef CONFIG_XPOWERS_CHIP_AXP202

#define XPOWERS_CHIP_AXP202
#include "XPowersLib.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

static const char *TAG = "AXP202";

#define AXP202_LOG_LINE_WIDTH   74
#define AXP202_LOG_LABEL_STATUS 24
#define AXP202_LOG_LABEL_RAIL   12
#define AXP202_LOG_LABEL_CONFIG 20

#define MY_AXP202_SLAVE_ADDRESS (0x34)
#define CHANNEL_ENABLE_ICON "✅"
#define CHANNEL_DISABLE_ICON "❌"

// REG82H bit4/5: ACIN current/voltage ADC (XPowersLib MONITOR_AC_* is private)
#define AXP202_ADC_ACIN_CURRENT (1U << 4)
#define AXP202_ADC_ACIN_VOLTAGE (1U << 5)

XPowersPMU power;

// LED2 软件状态；改状态后调用 axp202_gpio0_led_apply() 同步至 GPIO0 硬件
static bool s_gpio0_led_on = true;

static uint8_t axp202_gpio_ctl_reg(axp_gpio_t gpio)
{
    switch (gpio)
    {
    case AXP_GPIO_0:
        return XPOWERS_AXP202_GPIO0_CTL;
    case AXP_GPIO_1:
        return XPOWERS_AXP202_GPIO1_CTL;
    case AXP_GPIO_2:
        return XPOWERS_AXP202_GPIO2_CTL;
    case AXP_GPIO_3:
        return XPOWERS_AXP202_GPIO3_CTL;
    default:
        return 0;
    }
}

// 旧库 _axp202_gpio_*_select，类型见 axp20x.h
// https://github.com/lewisxhe/AXP202X_Library/blob/master/src/axp20x.h
static int axp202_gpio_mode_to_func(axp_gpio_t gpio, axp_gpio_mode_t mode)
{
    switch (gpio)
    {
    case AXP_GPIO_0:
        switch (mode)
        {
        case AXP_IO_OUTPUT_LOW_MODE:
            return 0;
        case AXP_IO_OUTPUT_HIGH_MODE:
            return 1;
        case AXP_IO_INPUT_MODE:
            return 2;
        case AXP_IO_LDO_MODE:
            return 3;
        case AXP_IO_ADC_MODE:
            return 4;
        default:
            return -1;
        }
    case AXP_GPIO_1:
        switch (mode)
        {
        case AXP_IO_OUTPUT_LOW_MODE:
            return 0;
        case AXP_IO_OUTPUT_HIGH_MODE:
            return 1;
        case AXP_IO_INPUT_MODE:
            return 2;
        case AXP_IO_ADC_MODE:
            return 4;
        default:
            return -1;
        }
    case AXP_GPIO_2:
        switch (mode)
        {
        case AXP_IO_OUTPUT_LOW_MODE:
            return 0;
        case AXP_IO_FLOATING_MODE:
            return 1;
        case AXP_IO_INPUT_MODE:
            return 2;
        default:
            return -1;
        }
    case AXP_GPIO_3:
        switch (mode)
        {
        case AXP_IO_OPEN_DRAIN_OUTPUT_MODE:
            return 0;
        case AXP_IO_INPUT_MODE:
            return 1;
        default:
            return -1;
        }
    default:
        return -1;
    }
}

// 旧库 setGPIOMode → _axp202_gpio_set
// https://github.com/lewisxhe/AXP202X_Library/blob/master/src/axp20x.cpp#L1683-L1727
esp_err_t axp202_setGPIOMode(axp_gpio_t gpio, axp_gpio_mode_t mode)
{
    uint8_t reg = axp202_gpio_ctl_reg(gpio);
    if (reg == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    int func = axp202_gpio_mode_to_func(gpio, mode);
    if (func < 0)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    int val = power.readRegister(reg);
    if (val < 0)
    {
        ESP_LOGE(TAG, "Failed to read GPIO%u_CTL", gpio);
        return ESP_FAIL;
    }

    if (gpio == AXP_GPIO_3)
    {
        val = func ? (val | 0x04) : (val & ~0x04);
    }
    else
    {
        val = (val & 0xF8) | (uint8_t)func;
    }

    if (power.writeRegister(reg, val) != 0)
    {
        ESP_LOGE(TAG, "Failed to write GPIO%u_CTL", gpio);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// 旧库 gpioWrite → _axp202_gpio_write
// https://github.com/lewisxhe/AXP202X_Library/blob/master/src/axp20x.cpp#L1839-L1871
esp_err_t axp202_gpioWrite(axp_gpio_t gpio, uint8_t val)
{
    uint8_t reg = axp202_gpio_ctl_reg(gpio);
    if (reg == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    int ctl = power.readRegister(reg);
    if (ctl < 0)
    {
        ESP_LOGE(TAG, "Failed to read GPIO%u_CTL", gpio);
        return ESP_FAIL;
    }

    if (gpio <= AXP_GPIO_1)
    {
        ctl = val ? (ctl | 0x01) : (ctl & 0xF8);
    }
    else if (gpio == AXP_GPIO_2)
    {
        if (val)
        {
            return ESP_ERR_NOT_SUPPORTED;
        }
        ctl = (ctl & 0xF8) | 0x00;
    }
    else if (gpio == AXP_GPIO_3)
    {
        if (val)
        {
            return ESP_ERR_NOT_SUPPORTED;
        }
        ctl = ctl & ~0x02;
    }

    if (power.writeRegister(reg, ctl) != 0)
    {
        ESP_LOGE(TAG, "Failed to write GPIO%u_CTL", gpio);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// 旧库 gpioRead → _axp202_gpio_read
// https://github.com/lewisxhe/AXP202X_Library/blob/master/src/axp20x.cpp#L1873-L1897
int axp202_gpioRead(axp_gpio_t gpio)
{
    if (gpio <= AXP_GPIO_2)
    {
        int val = power.readRegister(XPOWERS_AXP202_GPIO012_SIGNAL);
        if (val < 0)
        {
            return -1;
        }
        return (val >> (gpio + 4)) & 0x01;
    }

    if (gpio == AXP_GPIO_3)
    {
        int val = power.readRegister(XPOWERS_AXP202_GPIO3_CTL);
        if (val < 0)
        {
            return -1;
        }
        return !(val & 0x01);
    }

    return -1;
}

static const char *axp202_gpio012_func_name(uint8_t func)
{
    switch (func & 0x07)
    {
    case 0:
        return "Output Low";
    case 1:
        return "Output High";
    case 2:
        return "Input";
    case 3:
        return "LDOio";
    case 4:
        return "ADC";
    case 6:
    case 7:
        return "Floating";
    default:
        return "Unknown";
    }
}

static const char *axp202_gpio_mode_name(axp_gpio_t gpio)
{
    uint8_t reg = axp202_gpio_ctl_reg(gpio);
    if (reg == 0)
    {
        return "Invalid";
    }

    int val = power.readRegister(reg);
    if (val < 0)
    {
        return "Read Error";
    }

    if (gpio == AXP_GPIO_3)
    {
        return (val & 0x04) ? "Input" : "Open-Drain Output";
    }

    return axp202_gpio012_func_name((uint8_t)val);
}

static bool axp202_is_ldoio_enabled(void)
{
    int val = power.readRegister(XPOWERS_AXP202_GPIO0_CTL);
    if (val < 0)
    {
        return false;
    }
    return (val & 0x07) == 0x03;
}

static const char *axp202_ldo3_mode_name(void)
{
    return power.isLDO3LDOMode() ? "DCIN(<Vbus)" : "LDO";
}

static const char *axp202_chgled_mode_name(void)
{
    switch (power.getChargingLedMode())
    {
    case XPOWERS_CHG_LED_OFF:
        return "Off";
    case XPOWERS_CHG_LED_BLINK_1HZ:
        return "Blink 1Hz";
    case XPOWERS_CHG_LED_BLINK_4HZ:
        return "Blink 4Hz";
    case XPOWERS_CHG_LED_ON:
        return "On";
    case XPOWERS_CHG_LED_CTRL_CHG:
        return "Charge Ctrl";
    default:
        return "Unknown";
    }
}

static const char *axp202_vbus_vol_limit_name(uint8_t opt)
{
    switch (opt)
    {
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V:
        return "4.0V";
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V1:
        return "4.1V";
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V2:
        return "4.2V";
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V3:
        return "4.3V";
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V4:
        return "4.4V";
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V5:
        return "4.5V";
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V6:
        return "4.6V";
    case XPOWERS_AXP202_VBUS_VOL_LIM_4V7:
        return "4.7V";
    default:
        return "Unknown";
    }
}

static const char *axp202_vbus_cur_limit_name(uint8_t opt)
{
    switch (opt)
    {
    case XPOWERS_AXP202_VBUS_CUR_LIM_900MA:
        return "900mA";
    case XPOWERS_AXP202_VBUS_CUR_LIM_500MA:
        return "500mA";
    case XPOWERS_AXP202_VBUS_CUR_LIM_100MA:
        return "100mA";
    case XPOWERS_AXP202_VBUS_CUR_LIM_OFF:
        return "OFF";
    default:
        return "Unknown";
    }
}

static const char *axp202_chg_target_vol_name(uint8_t opt)
{
    switch (opt)
    {
    case XPOWERS_AXP202_CHG_VOL_4V1:
        return "4.1V";
    case XPOWERS_AXP202_CHG_VOL_4V15:
        return "4.15V";
    case XPOWERS_AXP202_CHG_VOL_4V2:
        return "4.2V";
    case XPOWERS_AXP202_CHG_VOL_4V36:
        return "4.36V";
    default:
        return "Unknown";
    }
}

static const char *axp202_chg_const_curr_name(uint8_t opt)
{
    switch (opt)
    {
    case XPOWERS_AXP202_CHG_CUR_100MA:
        return "100mA";
    case XPOWERS_AXP202_CHG_CUR_190MA:
        return "190mA";
    case XPOWERS_AXP202_CHG_CUR_280MA:
        return "280mA";
    case XPOWERS_AXP202_CHG_CUR_360MA:
        return "360mA";
    case XPOWERS_AXP202_CHG_CUR_450MA:
        return "450mA";
    case XPOWERS_AXP202_CHG_CUR_550MA:
        return "550mA";
    case XPOWERS_AXP202_CHG_CUR_630MA:
        return "630mA";
    case XPOWERS_AXP202_CHG_CUR_700MA:
        return "700mA";
    case XPOWERS_AXP202_CHG_CUR_780MA:
        return "780mA";
    case XPOWERS_AXP202_CHG_CUR_880MA:
        return "880mA";
    case XPOWERS_AXP202_CHG_CUR_960MA:
        return "960mA";
    case XPOWERS_AXP202_CHG_CUR_1000MA:
        return "1000mA";
    case XPOWERS_AXP202_CHG_CUR_1080MA:
        return "1080mA";
    case XPOWERS_AXP202_CHG_CUR_1160MA:
        return "1160mA";
    case XPOWERS_AXP202_CHG_CUR_1240MA:
        return "1240mA";
    case XPOWERS_AXP202_CHG_CUR_1320MA:
        return "1320mA";
    default:
        return "Unknown";
    }
}

static const char *axp202_active_input_name(void)
{
    if (power.isAcinEfficient())
    {
        return "ACIN";
    }
    if (power.isVbusIn() && power.getVbusCurrent() > 0.0f)
    {
        return "VBUS";
    }
    if (!power.isAcinIn() && !power.isVbusIn() && power.isBatteryConnect())
    {
        return "BAT";
    }
    return "UNKNOWN";
}

static void axp202_log_padded_line(const char *prefix, char pad_char)
{
    char line[AXP202_LOG_LINE_WIDTH + 1];
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    if (prefix_len > AXP202_LOG_LINE_WIDTH)
    {
        prefix_len = AXP202_LOG_LINE_WIDTH;
    }
    if (prefix_len > 0)
    {
        memcpy(line, prefix, prefix_len);
    }
    memset(line + prefix_len, pad_char, AXP202_LOG_LINE_WIDTH - prefix_len);
    line[AXP202_LOG_LINE_WIDTH] = '\0';
    ESP_LOGI(TAG, "%s", line);
}

static void axp202_log_banner(const char *title)
{
    axp202_log_padded_line(title, '=');
}

static void axp202_log_footer(void)
{
    char line[AXP202_LOG_LINE_WIDTH + 2];

    memset(line, '=', AXP202_LOG_LINE_WIDTH);
    line[AXP202_LOG_LINE_WIDTH] = '\n';
    line[AXP202_LOG_LINE_WIDTH + 1] = '\0';
    ESP_LOGI(TAG, "%s", line);
}

static void axp202_log_runtime_banner(void)
{
    const char *title = "AXP202";
    const size_t title_len = strlen(title);
    const size_t pad_total = AXP202_LOG_LINE_WIDTH - title_len;
    const size_t pad_left = pad_total / 2;
    char line[AXP202_LOG_LINE_WIDTH + 1];

    memset(line, '=', pad_left);
    memcpy(line + pad_left, title, title_len);
    memset(line + pad_left + title_len, '=', AXP202_LOG_LINE_WIDTH - pad_left - title_len);
    line[AXP202_LOG_LINE_WIDTH] = '\0';
    ESP_LOGI(TAG, "%s", line);
}

static void axp202_log_section(unsigned n, unsigned total, const char *title)
{
    char prefix[48];
    snprintf(prefix, sizeof(prefix), "[%u/%u] %s ", n, total, title);
    axp202_log_padded_line(prefix, '-');
}

static void axp202_log_field_w(unsigned idx, unsigned label_w, const char *label, const char *fmt, ...)
{
    char value[128];
    va_list args;

    va_start(args, fmt);
    vsnprintf(value, sizeof(value), fmt, args);
    va_end(args);
    ESP_LOGI(TAG, "  %u. %-*s : %s", idx, label_w, label, value);
}

static const char *axp202_poweroff_time_name(uint8_t opt)
{
    switch (opt)
    {
    case XPOWERS_POWEROFF_4S:
        return "4 Second";
    case XPOWERS_POWEROFF_6S:
        return "6 Second";
    case XPOWERS_POWEROFF_8S:
        return "8 Second";
    case XPOWERS_POWEROFF_10S:
        return "10 Second";
    default:
        return "Unknown";
    }
}

static const char *axp202_poweron_time_name(uint8_t opt)
{
    switch (opt)
    {
    case XPOWERS_POWERON_128MS:
        return "128 Ms";
    case XPOWERS_POWERON_512MS:
        return "512 Ms";
    case XPOWERS_POWERON_1S:
        return "1 Second";
    case XPOWERS_POWERON_2S:
        return "2 Second";
    default:
        return "Unknown";
    }
}

static void axp202_log_rail(unsigned idx, const char *name, bool enabled, uint16_t mv, const char *mode)
{
    const char *icon = enabled ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON;

    if (mode != nullptr)
    {
        axp202_log_field_w(idx, AXP202_LOG_LABEL_RAIL, name, "ENABLE %s    Voltage : %4u mV    Mode : %s", icon, mv, mode);
    }
    else
    {
        axp202_log_field_w(idx, AXP202_LOG_LABEL_RAIL, name, "ENABLE %s    Voltage : %4u mV", icon, mv);
    }
}

static void axp202_log_pmu_config(void)
{
    axp202_log_banner("PMU Config (Input/Charge)");
    axp202_log_section(1, 1, "Input / Charge");
    axp202_log_field_w(1, AXP202_LOG_LABEL_CONFIG, "SysPowerDownVoltage", "%u mV", power.getSysPowerDownVoltage());
    axp202_log_field_w(2, AXP202_LOG_LABEL_CONFIG, "VbusVoltageLimit", "%s",
                       axp202_vbus_vol_limit_name(power.getVbusVoltageLimit()));
    axp202_log_field_w(3, AXP202_LOG_LABEL_CONFIG, "VbusCurrentLimit", "%s",
                       axp202_vbus_cur_limit_name(power.getVbusCurrentLimit()));
    axp202_log_field_w(4, AXP202_LOG_LABEL_CONFIG, "ChargeTargetVoltage", "%s",
                       axp202_chg_target_vol_name(power.getChargeTargetVoltage()));
    axp202_log_field_w(5, AXP202_LOG_LABEL_CONFIG, "ChargerConstantCurr", "%s",
                       axp202_chg_const_curr_name(power.getChargerConstantCurr()));
    axp202_log_footer();
}

static void axp202_log_output_channels(void)
{
    axp202_log_banner("PMU Output (Rails/GPIO)");
    axp202_log_section(1, 3, "DCDC");
    axp202_log_rail(1, "DC2", power.isEnableDC2(), power.getDC2Voltage(), nullptr);
    axp202_log_rail(2, "DC3", power.isEnableDC3(), power.getDC3Voltage(), nullptr);
    axp202_log_section(2, 3, "LDO");
    axp202_log_rail(1, "LDO2", power.isEnableLDO2(), power.getLDO2Voltage(), nullptr);
    axp202_log_rail(2, "LDO3", power.isEnableLDO3(), power.getLDO3Voltage(), axp202_ldo3_mode_name());
    axp202_log_rail(3, "LDO4", power.isEnableLDO4(), power.getLDO4Voltage(), nullptr);
    axp202_log_rail(4, "LDOio", axp202_is_ldoio_enabled(), power.getLDOioVoltage(), axp202_gpio_mode_name(AXP_GPIO_0));
    axp202_log_section(3, 3, "GPIO");
    axp202_log_field_w(1, AXP202_LOG_LABEL_RAIL, "GPIO0(LDOio)", "Mode : %s", axp202_gpio_mode_name(AXP_GPIO_0));
    axp202_log_field_w(2, AXP202_LOG_LABEL_RAIL, "GPIO1", "Mode : %s", axp202_gpio_mode_name(AXP_GPIO_1));
    axp202_log_field_w(3, AXP202_LOG_LABEL_RAIL, "GPIO2", "Mode : %s", axp202_gpio_mode_name(AXP_GPIO_2));
    axp202_log_field_w(4, AXP202_LOG_LABEL_RAIL, "GPIO3", "Mode : %s", axp202_gpio_mode_name(AXP_GPIO_3));
    axp202_log_field_w(5, AXP202_LOG_LABEL_RAIL, "CHGLED", "Mode : %s", axp202_chgled_mode_name());
    axp202_log_footer();
}

static void axp202_log_power_key(void)
{
    axp202_log_banner("Power Key");
    axp202_log_section(1, 1, "Timing");
    axp202_log_field_w(1, AXP202_LOG_LABEL_CONFIG, "PowerKeyPressOffTime", "%s",
                       axp202_poweroff_time_name(power.getPowerKeyPressOffTime()));
    axp202_log_field_w(2, AXP202_LOG_LABEL_CONFIG, "PowerKeyPressOnTime", "%s",
                       axp202_poweron_time_name(power.getPowerKeyPressOnTime()));
    axp202_log_footer();
}

static void axp202_gpio0_led_apply(void)
{
    axp202_gpioWrite(AXP_GPIO_0, s_gpio0_led_on ? 1 : 0);
}

static void axp202_gpio0_led_toggle(void)
{
    s_gpio0_led_on = !s_gpio0_led_on;
    axp202_gpio0_led_apply();
}

esp_err_t axp202_init()
{
    axp202_log_runtime_banner();

    ESP_LOGW(TAG, "MY_AXP202_SLAVE_ADDRESS: 0x%x", MY_AXP202_SLAVE_ADDRESS);

    //* Implemented using read and write callback methods, applicable to other platforms
#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW
    ESP_LOGI(TAG, "Implemented using read and write callback methods");
    if (power.begin(MY_AXP202_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte))
    {
        ESP_LOGI(TAG, "Init AXP202 SUCCESS!");
    }
    else
    {
        ESP_LOGE(TAG, "Init AXP202 FAILED!");
        return ESP_FAIL;
    }
#endif

    //* Use the built-in esp-idf communication method
#if CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) && defined(CONFIG_XPOWERS_ESP_IDF_NEW_API)

    ESP_LOGI(TAG, "Implemented using built-in read and write methods (Use higher version >= 5.0 API)");
    // * Using the new API of esp-idf 5.x, you need to pass the I2C BUS handle,
    // * which is useful when the bus shares multiple devices.
    extern i2c_master_bus_handle_t bus_handle;

    if (power.begin(bus_handle, MY_AXP202_SLAVE_ADDRESS))
    {
        ESP_LOGI(TAG, "Init AXP202 SUCCESS!");
    }
    else
    {
        ESP_LOGE(TAG, "Init AXP202 FAILED!");
        return false;
    }
#else

    ESP_LOGI(TAG, "Implemented using built-in read and write methods (Use lower version < 5.0 API)");

    if (power.begin((i2c_port_t)CONFIG_I2C_MASTER_PORT_NUM, MY_AXP202_SLAVE_ADDRESS, CONFIG_PMU_I2C_SDA, CONFIG_PMU_I2C_SCL))
    {
        ESP_LOGI(TAG, "Init AXP202 SUCCESS!");
    }
    else
    {
        ESP_LOGE(TAG, "Init AXP202 FAILED!");
        return false;
    }
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
#endif // CONFIG_I2C_COMMUNICATION_METHOD_BUILTIN_RW

    ESP_LOGI(TAG, "getID: 0x%x", power.getChipID());

    axp202_log_footer();

    // Set the minimum system operating voltage inside the PMU,
    // below this value will shut down the PMU
    // Range: 2600~3300mV
    power.setSysPowerDownVoltage(2700);

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    power.setVbusVoltageLimit(XPOWERS_AXP202_VBUS_VOL_LIM_4V5);

    // Turn off USB input current limit
    power.setVbusCurrentLimit(XPOWERS_AXP202_VBUS_CUR_LIM_OFF);

    // DC2 700~2275 mV, 25mV/step, IMAX=1.6A
    // power.setDC2Voltage(3300); // 十轴传感器供电

    // DC3 700~3500 mV, 25mV/step, IMAX=1.2A
    power.setDC3Voltage(3300); // 主控供电

    // LDO2 1800~3300 mV, 100mV/step, IMAX=200mA
    power.setLDO2Voltage(3300); // 屏幕与SD卡供电

    // LDO3 700~3500 mV, 25mV/step, IMAX=200mA
    // power.setLDO3Mode(XPOWERS_AXP202_LDO3_MODE_LDO);
    // power.setLDO3Mode(XPOWERS_AXP202_LDO3_MODE_DCIN);
    power.setLDO3Voltage(3500); // 触觉驱动器供电

    // LDO4 1800~3300 mV, 100mV/step, IMAX=200mA
    /* LDO4 Range:
       1250, 1300, 1400, 1500, 1600, 1700, 1800, 1900,
       2000, 2500, 2700, 2800, 3000, 3100, 3200, 3300
    */
    // power.setLDO4Voltage(3300); // 音频供电

    // LDOio 1800~3300 mV, 100mV/step, IMAX=50mA
    // GPIO0 已用于 LED2 数字输出，暂不启用 LDOio
    // power.setLDOioVoltage(3300);

    // Enable power output channel
    power.disableDC2();
    power.enableDC3();
    power.enableLDO2();
    power.enableLDO3();
    power.disableLDO4();
    // power.enableLDOio();

    // GPIO0 用于 LED2（PMU_GPIO0，高电平点亮），默认亮（s_gpio0_led_on），PowerKey 短按 gpioWrite 切换
    axp202_setGPIOMode(AXP_GPIO_0, s_gpio0_led_on ? AXP_IO_OUTPUT_HIGH_MODE : AXP_IO_OUTPUT_LOW_MODE);

    // Set the time of pressing the button to turn off
    power.setPowerKeyPressOffTime(XPOWERS_POWEROFF_10S);

    // Set the button power-on press time
    power.setPowerKeyPressOnTime(XPOWERS_POWERON_2S);
    axp202_log_power_key();

    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    power.disableTSPinMeasure();

    power.enableTemperatureMeasure();
    // power.disableTemperatureMeasure();

    // Enable internal ADC detection
    power.enableBattDetection();
    power.enableVbusVoltageMeasure();
    power.enableAdcChannel(AXP202_ADC_ACIN_CURRENT | AXP202_ADC_ACIN_VOLTAGE);
    power.enableBattVoltageMeasure();
    power.enableSystemVoltageMeasure();

    /*
      The default setting is CHGLED is automatically controlled by the power.
    - XPOWERS_CHG_LED_OFF,
    - XPOWERS_CHG_LED_BLINK_1HZ,
    - XPOWERS_CHG_LED_BLINK_4HZ,
    - XPOWERS_CHG_LED_ON,
    - XPOWERS_CHG_LED_CTRL_CHG,
    * */
    power.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);

    // Disable all interrupts
    power.disableIRQ(XPOWERS_AXP202_ALL_IRQ);
    // Clear all interrupt flags
    power.clearIrqStatus();
    // Enable the required interrupt function
    power.enableIRQ(
        XPOWERS_AXP202_BAT_INSERT_IRQ | XPOWERS_AXP202_BAT_REMOVE_IRQ |      // BATTERY
        XPOWERS_AXP202_ACIN_CONNECT_IRQ | XPOWERS_AXP202_ACIN_REMOVED_IRQ |  // ACIN
        XPOWERS_AXP202_VBUS_INSERT_IRQ | XPOWERS_AXP202_VBUS_REMOVE_IRQ |    // VBUS
        XPOWERS_AXP202_PKEY_SHORT_IRQ | XPOWERS_AXP202_PKEY_LONG_IRQ |       // POWER KEY
        XPOWERS_AXP202_BAT_CHG_DONE_IRQ | XPOWERS_AXP202_BAT_CHG_START_IRQ | // CHARGE
        // XPOWERS_AXP202_PKEY_NEGATIVE_IRQ | XPOWERS_AXP202_PKEY_POSITIVE_IRQ   |   //POWER KEY
        XPOWERS_AXP202_TIMER_TIMEOUT_IRQ // Timer
    );

    // Set constant current charge current limit
    power.setChargerConstantCurr(XPOWERS_AXP202_CHG_CUR_280MA);
    // Set stop charging termination current
    power.setChargerTerminationCurr(XPOWERS_AXP202_CHG_ITERM_LESS_10_PERCENT);

    // Set charge cut-off voltage
    power.setChargeTargetVoltage(XPOWERS_AXP202_CHG_VOL_4V2);

    axp202_log_pmu_config();
    axp202_log_output_channels();

    // Cache writes and reads, as long as the PMU remains powered, the data will always be stored inside the PMU
    ESP_LOGI(TAG, "Write pmu data buffer");
    uint8_t data[XPOWERS_AXP202_DATA_BUFFER_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    power.writeDataBuffer(data, XPOWERS_AXP202_DATA_BUFFER_SIZE);
    memset(data, 0, XPOWERS_AXP202_DATA_BUFFER_SIZE);

    ESP_LOGI(TAG, "Read pmu data buffer");
    power.readDataBuffer(data, XPOWERS_AXP202_DATA_BUFFER_SIZE);
    ESP_LOG_BUFFER_HEX(TAG, data, XPOWERS_AXP202_DATA_BUFFER_SIZE);

    axp202_log_footer();

    // Set the timing after one minute, the isWdtExpireIrq will be triggered in the loop interrupt function
    power.setTimerout(1);

    return ESP_OK;
}

void axp202_show_info()
{
    axp202_log_runtime_banner();

    axp202_log_section(1, 4, "Power Path");
    axp202_log_field_w(1, AXP202_LOG_LABEL_STATUS, "ActiveInput", "%s", axp202_active_input_name());
    axp202_log_field_w(2, AXP202_LOG_LABEL_STATUS, "STATUS", "0x%02X", power.status());
    axp202_log_field_w(3, AXP202_LOG_LABEL_STATUS, "isVbusIn", "%s",
                       power.isVbusIn() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    axp202_log_field_w(4, AXP202_LOG_LABEL_STATUS, "isAcinIn", "%s",
                       power.isAcinIn() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    axp202_log_field_w(5, AXP202_LOG_LABEL_STATUS, "isAcinEfficient", "%s",
                       power.isAcinEfficient() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    axp202_log_field_w(6, AXP202_LOG_LABEL_STATUS, "isAcinVbusStart", "%s",
                       power.isAcinVbusStart() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);

    axp202_log_section(2, 4, "Input ADC");
    axp202_log_field_w(1, AXP202_LOG_LABEL_STATUS, "VBUS Voltage", "%4d mV    Current : %6.2f mA",
                       power.getVbusVoltage(), power.getVbusCurrent());
    axp202_log_field_w(2, AXP202_LOG_LABEL_STATUS, "ACIN Voltage", "%4d mV    Current : %6.2f mA",
                       power.getAcinVoltage(), power.getAcinCurrent());

    axp202_log_section(3, 4, "System");
    axp202_log_field_w(1, AXP202_LOG_LABEL_STATUS, "IPSOUT Voltage", "%d mV", power.getSystemVoltage());
    axp202_log_field_w(2, AXP202_LOG_LABEL_STATUS, "Temperature", "%.2f °C", power.getTemperature());
    axp202_log_field_w(3, AXP202_LOG_LABEL_STATUS, "isOverTemperature", "%s",
                       power.isOverTemperature() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);

    axp202_log_section(4, 4, "Battery");
    axp202_log_field_w(1, AXP202_LOG_LABEL_STATUS, "isBatteryConnect", "%s",
                       power.isBatteryConnect() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    axp202_log_field_w(2, AXP202_LOG_LABEL_STATUS, "isCharging", "%s",
                       power.isCharging() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    axp202_log_field_w(3, AXP202_LOG_LABEL_STATUS, "isDischarge", "%s",
                       power.isDischarge() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    axp202_log_field_w(4, AXP202_LOG_LABEL_STATUS, "isChargeCurrLessPreset", "%s",
                       power.isChargeCurrLessPreset() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    axp202_log_field_w(5, AXP202_LOG_LABEL_STATUS, "isBattInActiveMode", "%s",
                       power.isBattInActiveMode() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);

    if (power.isBatteryConnect())
    {
        axp202_log_field_w(6, AXP202_LOG_LABEL_STATUS, "Batt Voltage", "%d mV    Percent : %d %%",
                             power.getBattVoltage(), power.getBatteryPercent());
        axp202_log_field_w(7, AXP202_LOG_LABEL_STATUS, "Batt Discharge", "%.2f mA    Charge : %.2f mA",
                           power.getBattDischargeCurrent(), power.getBatteryChargeCurrent());
    }

    axp202_log_footer();
}

void axp202_enter_sleep()
{
    // Set sleep flag
    power.enableSleep();

    power.disableDC2();

    power.disableLDO2();
    power.disableLDO3();

    // Finally, turn off the power of the control chip
    power.disableDC3();
}

void axp202_isr_handler()
{
    // Get PMU Interrupt Status Register
    power.getIrqStatus();

    if (power.isAcinOverVoltageIrq())
    {
        ESP_LOGI(TAG, "isAcinOverVoltageIrq");
    }
    if (power.isAcinInserIrq())
    {
        ESP_LOGI(TAG, "isAcinInserIrq");
    }
    if (power.isAcinRemoveIrq())
    {
        ESP_LOGI(TAG, "isAcinRemoveIrq");
    }
    if (power.isVbusOverVoltageIrq())
    {
        ESP_LOGI(TAG, "isVbusOverVoltageIrq");
    }
    if (power.isVbusInsertIrq())
    {
        ESP_LOGI(TAG, "isVbusInsertIrq");
    }
    if (power.isVbusRemoveIrq())
    {
        ESP_LOGI(TAG, "isVbusRemoveIrq");
    }
    if (power.isVbusLowVholdIrq())
    {
        ESP_LOGI(TAG, "isVbusLowVholdIrq");
    }
    if (power.isBatInsertIrq())
    {
        ESP_LOGI(TAG, "isBatInsertIrq");
    }
    if (power.isBatRemoveIrq())
    {
        ESP_LOGI(TAG, "isBatRemoveIrq");
    }
    if (power.isBattEnterActivateIrq())
    {
        ESP_LOGI(TAG, "isBattEnterActivateIrq");
    }
    if (power.isBattExitActivateIrq())
    {
        ESP_LOGI(TAG, "isBattExitActivateIrq");
    }
    if (power.isBatChargeStartIrq())
    {
        ESP_LOGI(TAG, "isBatChargeStartIrq");
    }
    if (power.isBatChargeDoneIrq())
    {
        ESP_LOGI(TAG, "isBatChargeDoneIrq");
    }
    if (power.isBattTempHighIrq())
    {
        ESP_LOGI(TAG, "isBattTempHighIrq");
    }
    if (power.isBattTempLowIrq())
    {
        ESP_LOGI(TAG, "isBattTempLowIrq");
    }
    if (power.isChipOverTemperatureIrq())
    {
        ESP_LOGI(TAG, "isChipOverTemperatureIrq");
    }
    if (power.isChargingCurrentLessIrq())
    {
        ESP_LOGI(TAG, "isChargingCurrentLessIrq");
    }
    if (power.isDC1VoltageLessIrq())
    {
        ESP_LOGI(TAG, "isDC1VoltageLessIrq");
    }
    if (power.isDC2VoltageLessIrq())
    {
        ESP_LOGI(TAG, "isDC2VoltageLessIrq");
    }
    if (power.isDC3VoltageLessIrq())
    {
        ESP_LOGI(TAG, "isDC3VoltageLessIrq");
    }
    if (power.isPekeyShortPressIrq())
    {
        ESP_LOGI(TAG, "isPekeyShortPress");

        axp202_gpio0_led_toggle();
        ESP_LOGI(TAG, "LED2 toggled: %s", s_gpio0_led_on ? "ON" : "OFF");

        axp202_show_info();

        // enterPmuSleep();
    }
    if (power.isPekeyLongPressIrq())
    {
        ESP_LOGI(TAG, "isPekeyLongPress");
    }
    if (power.isNOEPowerOnIrq())
    {
        ESP_LOGI(TAG, "isNOEPowerOnIrq");
    }
    if (power.isNOEPowerDownIrq())
    {
        ESP_LOGI(TAG, "isNOEPowerDownIrq");
    }
    if (power.isVbusEffectiveIrq())
    {
        ESP_LOGI(TAG, "isVbusEffectiveIrq");
    }
    if (power.isVbusInvalidIrq())
    {
        ESP_LOGI(TAG, "isVbusInvalidIrq");
    }
    if (power.isVbusSessionIrq())
    {
        ESP_LOGI(TAG, "isVbusSessionIrq");
    }
    if (power.isVbusSessionEndIrq())
    {
        ESP_LOGI(TAG, "isVbusSessionEndIrq");
    }
    if (power.isLowVoltageLevel2Irq())
    {
        ESP_LOGI(TAG, "isLowVoltageLevel2Irq");
    }
    if (power.isWdtExpireIrq())
    {
        ESP_LOGI(TAG, "isWdtExpire");

        axp202_show_info();

        // Clear the timer state and continue to the next timer
        power.clearTimerFlag();
    }
    if (power.isGpio2EdgeTriggerIrq())
    {
        ESP_LOGI(TAG, "isGpio2EdgeTriggerIrq");
    }
    if (power.isGpio1EdgeTriggerIrq())
    {
        ESP_LOGI(TAG, "isGpio1EdgeTriggerIrq");
    }
    if (power.isGpio0EdgeTriggerIrq())
    {
        ESP_LOGI(TAG, "isGpio0EdgeTriggerIrq");
    }
    // Clear PMU Interrupt Status Register
    power.clearIrqStatus();
}

// 读取一份面向 UI 的 AXP202 实时快照（封装内部 C++ XPowersPMU 对象）。
esp_err_t axp202_read_dashboard(axp202_dashboard_t *out)
{
    if (out == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    // 电源输出轨：电压(mV) 与使能状态
    out->dcdc2_mv = power.getDC2Voltage();
    out->dcdc3_mv = power.getDC3Voltage();
    out->ldo2_mv = power.getLDO2Voltage();
    out->ldo3_mv = power.getLDO3Voltage();
    out->ldo4_mv = power.getLDO4Voltage();
    out->dcdc2_on = power.isEnableDC2();
    out->dcdc3_on = power.isEnableDC3();
    out->ldo2_on = power.isEnableLDO2();
    out->ldo3_on = power.isEnableLDO3();
    out->ldo4_on = power.isEnableLDO4();
    out->exten_on = power.isEnableExternalPin();

    // 电池
    out->batt_connected = power.isBatteryConnect();
    out->charging = power.isCharging();
    out->vbat_v = power.getBattVoltage() / 1000.0f;
    out->batt_pct = power.getBatteryPercent();
    // 充电时取充电电流(正)，否则取放电电流并记为负值
    out->ibat_ma = out->charging ? power.getBatteryChargeCurrent()
                                 : -power.getBattDischargeCurrent();

    // 输入电源
    out->acin_in = power.isAcinIn();
    out->acin_v = power.getAcinVoltage() / 1000.0f;
    out->acin_ma = power.getAcinCurrent();
    out->vbus_in = power.isVbusIn();
    out->vbus_v = power.getVbusVoltage() / 1000.0f;
    out->vbus_ma = power.getVbusCurrent();

    // 芯片温度
    out->temp_c = power.getTemperature();

    return ESP_OK;
}

// 使能/关闭指定电源输出轨（供 UI 电源开关面板调用）。
esp_err_t axp202_set_rail(axp202_rail_t rail, bool on)
{
    bool ok;
    switch (rail)
    {
    case AXP202_RAIL_DCDC2:
        ok = on ? power.enableDC2() : power.disableDC2();
        break;
    case AXP202_RAIL_DCDC3:
        ok = on ? power.enableDC3() : power.disableDC3();
        break;
    case AXP202_RAIL_LDO2:
        ok = on ? power.enableLDO2() : power.disableLDO2();
        break;
    case AXP202_RAIL_LDO3:
        ok = on ? power.enableLDO3() : power.disableLDO3();
        break;
    case AXP202_RAIL_LDO4:
        ok = on ? power.enableLDO4() : power.disableLDO4();
        break;
    case AXP202_RAIL_EXTEN:
        ok = on ? power.enableExternalPin() : power.disableExternalPin();
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    if (!ok)
    {
        ESP_LOGE(TAG, "axp202_set_rail(%d, %d) failed", (int)rail, (int)on);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "axp202_set_rail(%d) -> %s", (int)rail, on ? "ON" : "OFF");
    return ESP_OK;
}

#endif /*CONFIG_XPOWERS_CHIP_AXP202*/
