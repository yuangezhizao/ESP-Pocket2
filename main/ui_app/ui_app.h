/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 可移植 UI 层：仅依赖 LVGL，不含任何硬件/ESP-IDF 依赖，以便同一份界面代码在真机、QEMU、PC(SDL) 等后端复用。硬件数据源(如 RTC 时间)通过回调注入，实现 UI 与硬件解耦。
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* hp28008 目标画布尺寸（可移植、无硬件依赖），供 PC/QEMU 等非 ESP 后端创建显示/窗口用。
 * 数值须与真机 hp28008.h 的 EXAMPLE_LCD_H_RES/V_RES 保持一致（均为 hp28008 物理 240x320）。
 * 因 hp28008.h 含大量 ESP-IDF 头依赖、PC 无法 include，故此处另置同值宏（两处定义、约定同值，非同一处单一来源）。 */
#define UI_APP_HOR_RES 240
#define UI_APP_VER_RES 320

    /* 时钟文本数据源：返回当前要显示的时间字符串；UI 不关心其来源(真机读 RTC / PC 读系统时间等)。 */
    typedef const char *(*ui_clock_source_fn)(void);

    /*
     * 在指定 screen 上创建可移植 UI（logo + 欢迎语 + 时钟 + 旋转按钮）。
     * 一次性构建：重复调用会被运行时守卫忽略（打印 warning 并直接返回），以免重复创建 timer 造成旧 timer 悬挂。
     * scr: 目标屏幕对象(通常为 lv_screen_active())。
     * clock_src: 时钟文本提供者；传 NULL 时时钟显示占位串。
     */
    void ui_app_create(lv_obj_t *scr, ui_clock_source_fn clock_src);

#ifdef __cplusplus
}
#endif
