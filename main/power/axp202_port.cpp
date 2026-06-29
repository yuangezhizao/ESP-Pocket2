#include "axp202_port.h"

#ifdef CONFIG_XPOWERS_CHIP_AXP202

#define XPOWERS_CHIP_AXP202
#include "XPowersLib.h"
static const char *TAG = "AXP202";

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

static void axp202_log_output_channels(void)
{
    ESP_LOGI(TAG, "DCDC=======================================================================");
    ESP_LOGI(TAG, "DC2:     ENABLE: %s    Voltage:%u mV", power.isEnableDC2() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getDC2Voltage());
    ESP_LOGI(TAG, "DC3:     ENABLE: %s    Voltage:%u mV", power.isEnableDC3() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getDC3Voltage());
    ESP_LOGI(TAG, "LDO========================================================================");
    ESP_LOGI(TAG, "LDO2:    ENABLE: %s    Voltage:%u mV", power.isEnableLDO2() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDO2Voltage());
    ESP_LOGI(TAG, "LDO3:    ENABLE: %s    Voltage:%u mV    Mode: %s", power.isEnableLDO3() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDO3Voltage(), axp202_ldo3_mode_name());
    ESP_LOGI(TAG, "LDO4:    ENABLE: %s    Voltage:%u mV", power.isEnableLDO4() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDO4Voltage());
    ESP_LOGI(TAG, "LDOio:   ENABLE: %s    Voltage:%u mV    Mode: %s", axp202_is_ldoio_enabled() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDOioVoltage(), axp202_gpio_mode_name(AXP_GPIO_0));
    ESP_LOGI(TAG, "GPIO=======================================================================");
    ESP_LOGI(TAG, "GPIO0(LDOio): Mode: %s", axp202_gpio_mode_name(AXP_GPIO_0));
    ESP_LOGI(TAG, "GPIO1:   Mode: %s", axp202_gpio_mode_name(AXP_GPIO_1));
    ESP_LOGI(TAG, "GPIO2:   Mode: %s", axp202_gpio_mode_name(AXP_GPIO_2));
    ESP_LOGI(TAG, "GPIO3:   Mode: %s", axp202_gpio_mode_name(AXP_GPIO_3));
    ESP_LOGI(TAG, "CHGLED:  Mode: %s", axp202_chgled_mode_name());
    ESP_LOGI(TAG, "===========================================================================\n");
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
    ESP_LOGI(TAG, "===================================AXP202==================================");

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

    ESP_LOGI(TAG, "===========================================================================\n");

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
    uint8_t opt = power.getPowerKeyPressOffTime();
    ESP_LOGI(TAG, "PowerKeyPressOffTime:");
    switch (opt)
    {
    case XPOWERS_POWEROFF_4S:
        ESP_LOGI(TAG, "4 Second");
        break;
    case XPOWERS_POWEROFF_6S:
        ESP_LOGI(TAG, "6 Second");
        break;
    case XPOWERS_POWEROFF_8S:
        ESP_LOGI(TAG, "8 Second");
        break;
    case XPOWERS_POWEROFF_10S:
        ESP_LOGI(TAG, "10 Second");
        break;
    default:
        break;
    }

    // Set the button power-on press time
    power.setPowerKeyPressOnTime(XPOWERS_POWERON_2S);
    opt = power.getPowerKeyPressOnTime();
    ESP_LOGI(TAG, "PowerKeyPressOnTime:");
    switch (opt)
    {
    case XPOWERS_POWERON_128MS:
        ESP_LOGI(TAG, "128 Ms");
        break;
    case XPOWERS_POWERON_512MS:
        ESP_LOGI(TAG, "512 Ms");
        break;
    case XPOWERS_POWERON_1S:
        ESP_LOGI(TAG, "1 Second");
        break;
    case XPOWERS_POWERON_2S:
        ESP_LOGI(TAG, "2 Second");
        break;
    default:
        break;
    }

    ESP_LOGI(TAG, "===========================================================================\n");

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

    axp202_log_output_channels();

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

    // Cache writes and reads, as long as the PMU remains powered, the data will always be stored inside the PMU
    ESP_LOGI(TAG, "Write pmu data buffer");
    uint8_t data[XPOWERS_AXP202_DATA_BUFFER_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    power.writeDataBuffer(data, XPOWERS_AXP202_DATA_BUFFER_SIZE);
    memset(data, 0, XPOWERS_AXP202_DATA_BUFFER_SIZE);

    ESP_LOGI(TAG, "Read pmu data buffer");
    power.readDataBuffer(data, XPOWERS_AXP202_DATA_BUFFER_SIZE);
    ESP_LOG_BUFFER_HEX(TAG, data, XPOWERS_AXP202_DATA_BUFFER_SIZE);

    ESP_LOGI(TAG, "===========================================================================\n");

    // Set the timing after one minute, the isWdtExpireIrq will be triggered in the loop interrupt function
    power.setTimerout(1);

    return ESP_OK;
}

void axp202_show_info()
{
    ESP_LOGI(TAG, "===================================AXP202==================================");
    ESP_LOGI(TAG, "isCharging: %s", power.isCharging() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    ESP_LOGI(TAG, "isDischarge: %s", power.isDischarge() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    ESP_LOGI(TAG, "isVbusIn: %s", power.isVbusIn() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    ESP_LOGI(TAG, "isAcinIn: %s", power.isAcinIn() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    ESP_LOGI(TAG, "STATUS: 0x%02X", power.status());
    ESP_LOGI(TAG, "isAcinEfficient: %s", power.isAcinEfficient() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);
    ESP_LOGI(TAG, "isAcinVbusStart: %s", power.isAcinVbusStart() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON);

    ESP_LOGI(TAG, "getVbusVoltage: %d mV", power.getVbusVoltage());
    ESP_LOGI(TAG, "getVbusCurrent: %.2f mA", power.getVbusCurrent());
    ESP_LOGI(TAG, "getAcinVoltage: %d mV", power.getAcinVoltage());
    ESP_LOGI(TAG, "getAcinCurrent: %.2f mA", power.getAcinCurrent());
    ESP_LOGI(TAG, "getSystemVoltage: %d mV", power.getSystemVoltage());
    ESP_LOGI(TAG, "getTemperature: %.2f°C", power.getTemperature());

    if (power.isBatteryConnect())
    {
        ESP_LOGI(TAG, "getBattVoltage: %d mV", power.getBattVoltage());
        ESP_LOGI(TAG, "getBattDischargeCurrent: %.2f mA", power.getBattDischargeCurrent());
        ESP_LOGI(TAG, "getBatteryChargeCurrent: %.2f mA", power.getBatteryChargeCurrent());
        ESP_LOGI(TAG, "getBatteryPercent: %d %%", power.getBatteryPercent());
    }

    ESP_LOGI(TAG, "===========================================================================\n");
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

#endif /*CONFIG_XPOWERS_CHIP_AXP202*/
