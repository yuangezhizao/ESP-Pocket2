/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PC/SDL 模拟器入口：复用 main/ui_app 的可移植 UI 层，通过 lv_sdl_mouse 模拟触摸交互。
 */
#define SDL_MAIN_HANDLED   /* 修复 SDL 在部分平台的 WinMain/main 链接问题（LVGL 官方示例做法） */
#include "lvgl.h"
#include "ui_app.h"
#include <time.h>

/* PC 侧时钟数据源：用本机时间替代 BM8563 RTC；失败时返回占位串 */
static const char *pc_clock_source(void)
{
    static char buf[32];
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *t = localtime_r(&now, &tm_buf);   /* localtime_r is thread-safe (POSIX) */
    if (t == NULL || strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t) == 0)
    {
        return "----/--/-- --:--:--";
    }
    return buf;
}

int main(void)
{
    lv_init();
    /* 窗口尺寸对齐 hp28008，取自 ui_app.h 的可移植宏，避免写死 240/320 */
    lv_display_t *disp = lv_sdl_window_create(UI_APP_HOR_RES, UI_APP_VER_RES);
    (void)disp;
    lv_indev_t *mouse = lv_sdl_mouse_create();   /* 鼠标模拟触摸 */
    (void)mouse;

    ui_app_create(lv_screen_active(), pc_clock_source);   /* 仅调用一次 */

    while (1)
    {
        lv_timer_handler();
        lv_delay_ms(5);
    }
    return 0;
}
