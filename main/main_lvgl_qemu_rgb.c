/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * QEMU 专用 LVGL demo 入口。切换 demo 见 main/CMakeLists.txt 的 SRCS 注释。
 */
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl_qemu_rgb.h"

static const char *TAG = "MAIN-LVGL-QEMU-RGB";

void app_main(void)
{
    ESP_LOGI(TAG, "Start LVGL demo on QEMU virtual RGB panel");
    ESP_ERROR_CHECK(qemu_rgb_lvgl_run());
}
