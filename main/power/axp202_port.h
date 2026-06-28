#pragma once

#include "i2c_port.h"

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include <stdint.h>
#include "esp_log.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief AXP202 GPIO 引脚编号（兼容旧 AXP202X_Library axp_gpio_t）
     */
    typedef enum
    {
        AXP_GPIO_0,
        AXP_GPIO_1,
        AXP_GPIO_2,
        AXP_GPIO_3,
    } axp_gpio_t;

    /**
     * @brief AXP202 GPIO 功能模式（兼容旧 AXP202X_Library axp_gpio_mode_t）
     *
     * 各引脚支持情况（与旧库 AXP202X_Library 一致）：
     *
     * | Pin   | setGPIOMode modes               | gpioWrite |
     * | ----- | ------------------------------- | --------- |
     * | GPIO0 | OutLow,OutHigh,In,LDOio,ADC     | 0/1       |
     * | GPIO1 | OutLow,OutHigh,In,ADC           | 0/1       |
     * | GPIO2 | OutLow,In,Float                 | 0 only    |
     * | GPIO3 | OpenDrain,In                    | 0 only    |
     *
     * 上表 modes：OutLow=输出低, OutHigh=输出高, In=输入, Float=浮空,
     * OpenDrain=开漏输出, LDOio/ADC 同手册命名；gpioWrite「0 only」= 仅可拉低。
     *
     * 注：GPIO0 与 LDOio 共用 Pin17；LDOio 对应 AXP_IO_LDO_MODE（0x03）。
     *     板载 LED2 接 GPIO0，应使用 GPIO 数字输出模式，而非 LDOio。
     *
     * 旧库参考：https://github.com/lewisxhe/AXP202X_Library/blob/master/src/axp20x.h
     */
    typedef enum
    {
        AXP_IO_OUTPUT_LOW_MODE,
        AXP_IO_OUTPUT_HIGH_MODE,
        AXP_IO_INPUT_MODE,
        AXP_IO_LDO_MODE,
        AXP_IO_ADC_MODE,
        AXP_IO_FLOATING_MODE,
        AXP_IO_OPEN_DRAIN_OUTPUT_MODE,
    } axp_gpio_mode_t;

    esp_err_t axp202_init();
    esp_err_t axp202_setGPIOMode(axp_gpio_t gpio, axp_gpio_mode_t mode);
    esp_err_t axp202_gpioWrite(axp_gpio_t gpio, uint8_t val);
    int axp202_gpioRead(axp_gpio_t gpio);
    void axp202_show_info();
    void axp202_enter_sleep();
    void axp202_isr_handler();

#ifdef __cplusplus
}
#endif
