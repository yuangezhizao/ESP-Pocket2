/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 可移植 LVGL <-> LovyanGFX 桥接：建 lv_display/lv_indev + flush/read 回调。
 * 仅依赖 lvgl + LovyanGFX + ui_app.h，绝不依赖 esp-idf，真机与 PC 共用同一份。
 * lv_init / tick / lv_timer_handler 循环由各入口负责（平台相关运行时）。
 */
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "lvgl.h"
#include "ui_app.h"

/* draw buffer 高度（行），真机/PC 各自据此分配缓冲；禁止裸魔数（见 spec 编码规范） */
#define LVGL_LGFX_DRAW_BUFF_HEIGHT 50

/* 本头为纯 C++（cfg 含 lgfx:: 类型），仅供 .cpp include、不提供 C 接口，故不加 extern "C"。
 * 真正的 C↔C++ 边界是 lvgl_lgfx_hp28008.cpp 导出的 extern "C" lvgl_lgfx_hp28008_run()。 */

/* 触摸源：返回是否按下；按下时填「逻辑坐标系」的 x/y。由各平台注入。 */
typedef bool (*lvgl_lgfx_touch_read_fn)(int32_t *x, int32_t *y);

typedef struct {
    lgfx::LGFX_Device *lcd;            /* 注入的 LGFX 设备（真机 ST7789 / PC Panel_sdl） */
    void *buf1;                        /* draw buffer 1（各入口分配） */
    void *buf2;                        /* draw buffer 2（双缓冲） */
    uint32_t buf_size_bytes;           /* 每块字节数 */
    lvgl_lgfx_touch_read_fn touch_read; /* 触摸源，可为 NULL */
    ui_clock_source_fn clock_src;       /* 时钟源，可为 NULL */
} lvgl_lgfx_bridge_cfg_t;

/*
 * 建 lv_display + lv_indev + flush/read cb + ui_app_create（内部按 UI_APP_HOR_RES/VER_RES）。
 * 前置：调用方须已 lv_init()；若平台有独立 LVGL task/锁（真机）须在 LVGL 锁内调用；PC 单线程 setup 阶段无需额外锁。
 * 返回创建的 lv_display_t*（供入口读取旋转等）。
 */
lv_display_t *lvgl_lgfx_bridge_create(const lvgl_lgfx_bridge_cfg_t *cfg);
