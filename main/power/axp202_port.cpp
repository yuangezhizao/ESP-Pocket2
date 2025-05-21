#include "axp202_port.h"

#ifdef CONFIG_XPOWERS_CHIP_AXP202

#define XPOWERS_CHIP_AXP202
#include "XPowersLib.h"
static const char *TAG = "AXP202";

#define MY_AXP202_SLAVE_ADDRESS (0x34)
#define CHANNEL_ENABLE_ICON  "✅"
#define CHANNEL_DISABLE_ICON "❌"

XPowersPMU power;

esp_err_t pmu_init()
{
    ESP_LOGI(TAG, "===================================AXP202==================================");

    ESP_LOGW(TAG, "MY_AXP202_SLAVE_ADDRESS: 0x%x", MY_AXP202_SLAVE_ADDRESS);

    //* Implemented using read and write callback methods, applicable to other platforms
#if CONFIG_I2C_COMMUNICATION_METHOD_CALLBACK_RW
    ESP_LOGI(TAG, "Implemented using read and write callback methods");
    if (power.begin(MY_AXP202_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte))
    {
        ESP_LOGI(TAG, "Init PMU SUCCESS!");
    }
    else
    {
        ESP_LOGE(TAG, "Init PMU FAILED!");
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
        ESP_LOGI(TAG, "Init PMU SUCCESS!");
    }
    else
    {
        ESP_LOGE(TAG, "Init PMU FAILED!");
        return false;
    }
#else

    ESP_LOGI(TAG, "Implemented using built-in read and write methods (Use lower version < 5.0 API)");

    if (power.begin((i2c_port_t)CONFIG_I2C_MASTER_PORT_NUM, MY_AXP202_SLAVE_ADDRESS, CONFIG_PMU_I2C_SDA, CONFIG_PMU_I2C_SCL))
    {
        ESP_LOGI(TAG, "Init PMU SUCCESS!");
    }
    else
    {
        ESP_LOGE(TAG, "Init PMU FAILED!");
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
    // power.setLDO3Voltage(3500); // 触觉驱动器供电

    // LDO4 1800~3300 mV, 100mV/step, IMAX=200mA
    /* LDO4 Range:
       1250, 1300, 1400, 1500, 1600, 1700, 1800, 1900,
       2000, 2500, 2700, 2800, 3000, 3100, 3200, 3300
    */
    // power.setLDO4Voltage(3300); // 音频供电

    // LDOio 1800~3300 mV, 100mV/step, IMAX=50mA
    power.setLDOioVoltage(3300);

    // Enable power output channel
    power.disableDC2();
    power.enableDC3();
    power.enableLDO2();
    power.disableLDO3();
    power.disableLDO4();
    power.enableLDOio();

    ESP_LOGI(TAG, "DCDC=======================================================================");
    ESP_LOGI(TAG, "DC2:     ENABLE: %s    Voltage:%u mV", power.isEnableDC2() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getDC2Voltage());
    ESP_LOGI(TAG, "DC3:     ENABLE: %s    Voltage:%u mV", power.isEnableDC3() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getDC3Voltage());
    ESP_LOGI(TAG, "LDO========================================================================");
    ESP_LOGI(TAG, "LDO2:    ENABLE: %s    Voltage:%u mV", power.isEnableLDO2() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDO2Voltage());
    ESP_LOGI(TAG, "LDO3:    ENABLE: %s    Voltage:%u mV    Mode: %s", power.isEnableLDO3() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDO3Voltage(), power.isLDO3LDOMode() ? "DCIN(<Vbus)" : "LDO");
    ESP_LOGI(TAG, "LDO4:    ENABLE: %s    Voltage:%u mV", power.isEnableLDO4() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDO4Voltage());
    ESP_LOGI(TAG, "LDOio:   ENABLE: %s    Voltage:%u mV", power.isEnableLDOio() ? CHANNEL_ENABLE_ICON : CHANNEL_DISABLE_ICON, power.getLDOioVoltage());
    ESP_LOGI(TAG, "===========================================================================\n");

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

    // TODO
    // 打开主板指示灯PIO0
    // power.setGPIOMode(AXP_GPIO_0, AXP_IO_OUTPUT_HIGH_MODE); // 打开主板指示灯PIO0
    // power.gpioWrite(AXP_GPIO_0, 1);

    // Set the timing after one minute, the isWdtExpireIrq will be triggered in the loop interrupt function
    power.setTimerout(1);

    return ESP_OK;
}

void printPMU()
{
    ESP_LOGI(TAG, "===================================AXP202==================================");
    ESP_LOGI(TAG, "isCharging: %s", power.isCharging() ? "YES" : "NO");
    ESP_LOGI(TAG, "isDischarge: %s", power.isDischarge() ? "YES" : "NO");
    ESP_LOGI(TAG, "isVbusIn: %s", power.isVbusIn() ? "YES" : "NO");

    ESP_LOGI(TAG, "getVbusVoltage: %d mV", power.getVbusVoltage());
    ESP_LOGI(TAG, "getVbusCurrent: %.2f mA", power.getVbusCurrent());
    // ESP_LOGI(TAG, "getAcinVoltage: %d mV", power.getAcinVoltage());
    // ESP_LOGI(TAG, "getAcinCurrent: %.2f mA", power.getAcinCurrent());
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

void enterPmuSleep(void)
{
    // Set sleep flag
    power.enableSleep();

    power.disableDC2();

    power.disableLDO2();
    power.disableLDO3();

    // Finally, turn off the power of the control chip
    power.disableDC3();
}

void pmu_isr_handler()
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

        printPMU();

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
