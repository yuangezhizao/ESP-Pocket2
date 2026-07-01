/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 移植自官方例程 esp_lcd_qemu_rgb/examples/lcd_qemu_rgb_panel（LVGL v8 -> v9 适配）。
 */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在 QEMU 虚拟 RGB 面板上启动 LVGL 例程
 *
 * 建面板 + 自管 LVGL（draw buffer / esp_timer tick / LVGL task）+ 显示 UI。
 * 仅在 QEMU 中可用：真实硬件上 esp_lcd_new_rgb_qemu 会返回 ESP_ERR_NOT_SUPPORTED。
 */
esp_err_t qemu_rgb_lvgl_run(void);

/**
 * @brief UI 入口（定义在 lvgl_demo_ui.c），由 qemu_rgb_lvgl_run 在持锁状态下调用
 */
void qemu_rgb_lvgl_demo_ui(lv_display_t *disp);

#ifdef __cplusplus
}
#endif
