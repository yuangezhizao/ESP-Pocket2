#pragma once

#include "i2c_port.h"

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include <stdint.h>
#include <stdbool.h>
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

    /**
     * @brief 供 UI 使用的 AXP202 实时读数快照。
     *
     * 电压统一为伏特(V)、电流统一为毫安(mA)、温度为摄氏度(°C)。
     * 该结构体面向 C（LVGL 页面为 .c），封装了内部 C++ 的 XPowersPMU 对象。
     */
    typedef struct
    {
        /* 电源输出轨：电压(mV) 与使能状态 */
        int dcdc2_mv;
        int dcdc3_mv;
        int ldo2_mv;
        int ldo3_mv;
        int ldo4_mv;
        bool dcdc2_on;
        bool dcdc3_on;
        bool ldo2_on;
        bool ldo3_on;
        bool ldo4_on;
        bool exten_on; /* EXTEN 外部使能引脚 */

        /* 电池 */
        bool batt_connected;
        bool charging;
        float vbat_v;   /* V */
        int batt_pct;   /* 0~100，-1 表示未检测到电池 */
        float ibat_ma;  /* mA，正=充电电流，负=放电电流 */

        /* 输入电源 */
        bool acin_in;
        float acin_v;  /* V */
        float acin_ma; /* mA */
        bool vbus_in;
        float vbus_v;  /* V */
        float vbus_ma; /* mA */

        /* 芯片温度 */
        float temp_c; /* °C */
    } axp202_dashboard_t;

    esp_err_t axp202_init();
    esp_err_t axp202_setGPIOMode(axp_gpio_t gpio, axp_gpio_mode_t mode);
    esp_err_t axp202_gpioWrite(axp_gpio_t gpio, uint8_t val);
    int axp202_gpioRead(axp_gpio_t gpio);
    void axp202_show_info();
    void axp202_enter_sleep();
    void axp202_isr_handler();

    /**
     * @brief 读取 AXP202 各路电源/电池/输入/温度的实时快照。
     *
     * 需在 axp202_init() 成功之后调用（ADC 通道已在 init 中使能）。
     *
     * @param out 输出快照，不能为 NULL。
     * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数为空。
     */
    esp_err_t axp202_read_dashboard(axp202_dashboard_t *out);

    /**
     * @brief 可开关控制的电源输出轨标识（与 UI 电源开关面板一一对应）。
     */
    typedef enum
    {
        AXP202_RAIL_DCDC2 = 0,
        AXP202_RAIL_DCDC3,
        AXP202_RAIL_LDO2,
        AXP202_RAIL_LDO3,
        AXP202_RAIL_LDO4,
        AXP202_RAIL_EXTEN,
        AXP202_RAIL_COUNT,
    } axp202_rail_t;

    /**
     * @brief 使能/关闭指定电源输出轨。
     *
     * 警告：DCDC3 为主控供电、LDO2 为屏幕与 SD 卡供电，真机上关闭它们会导致
     *       系统断电或屏幕黑屏。调用方需自行确认风险。
     *
     * @param rail 电源轨标识。
     * @param on   true=使能，false=关闭。
     * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 非法轨；ESP_FAIL 底层写失败。
     */
    esp_err_t axp202_set_rail(axp202_rail_t rail, bool on);

#ifdef __cplusplus
}
#endif
