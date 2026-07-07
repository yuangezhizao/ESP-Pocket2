/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PC LGFX 模拟器：LovyanGFX Panel_sdl 后端渲染共享 ui_app（经 lvgl_lgfx_bridge），鼠标模拟触摸。
 */
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/sdl/Panel_sdl.hpp>
#include "lvgl.h"
#include "lvgl_lgfx_bridge.hpp"
#include "ui_app.h"
#include <ctime>

/* PC 侧 LGFX 设备：Panel_sdl，尺寸对齐 hp28008 */
class LGFX_PC : public lgfx::LGFX_Device
{
    lgfx::Panel_sdl _panel;
public:
    LGFX_PC(void)
    {
        auto cfg = _panel.config();
        cfg.memory_width  = UI_APP_HOR_RES;
        cfg.memory_height = UI_APP_VER_RES;
        cfg.panel_width   = UI_APP_HOR_RES;
        cfg.panel_height  = UI_APP_VER_RES;
        _panel.config(cfg);
        setPanel(&_panel);
    }
};

static LGFX_PC s_lcd;

/* 触摸源：Panel_sdl 的 getTouch 已随 setRotation 返回逻辑坐标（G7-B，PC 端自动） */
static bool pc_touch_read(int32_t *x, int32_t *y)
{
    int32_t tx = 0, ty = 0;
    if (s_lcd.getTouch(&tx, &ty)) {
        *x = tx;
        *y = ty;
        return true;
    }
    return false;
}

/* 时钟源：本机时间（POSIX localtime_r，线程安全）；失败返回占位串 */
static const char *pc_clock_source(void)
{
    static char buf[32];
    time_t now = time(NULL);
    struct tm tmv;
    struct tm *t = localtime_r(&now, &tmv);
    if (t == NULL || strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t) == 0) {
        return "----/--/-- --:--:--";
    }
    return buf;
}

/* LVGL tick 源：用 LovyanGFX 的 millis()（无捕获 lambda 可转函数指针） */
static uint32_t pc_tick_get(void)
{
    return (uint32_t)lgfx::millis();
}

bool setup(void)
{
    /* Panel_sdl::init() 返回 bool（恒真，同真机侧），此检查为防御性/与真机 setup 对称 */
    if (!s_lcd.init()) {
        LV_LOG_ERROR("LGFX Panel_sdl init failed");
        return false;
    }
    s_lcd.setColorDepth(16);

    lv_init();
    lv_tick_set_cb(pc_tick_get);

    static lv_color_t buf1[UI_APP_HOR_RES * LVGL_LGFX_DRAW_BUFF_HEIGHT];
    static lv_color_t buf2[UI_APP_HOR_RES * LVGL_LGFX_DRAW_BUFF_HEIGHT];

    lvgl_lgfx_bridge_cfg_t cfg = {};
    cfg.lcd = &s_lcd;
    cfg.buf1 = buf1;
    cfg.buf2 = buf2;
    cfg.buf_size_bytes = sizeof(buf1);
    cfg.touch_read = pc_touch_read;
    cfg.clock_src = pc_clock_source;
    if (lvgl_lgfx_bridge_create(&cfg) == NULL) {
        LV_LOG_ERROR("lvgl_lgfx_bridge_create failed");
        return false;
    }
    return true;
}

void loop(void)
{
    lv_timer_handler();
    lgfx::delay(5);
}

int user_func(bool *running)
{
    /* setup 失败：结束本用户线程（不再 loop → LVGL 停更）。注意 Panel_sdl::main 用
     * SDL_WaitThread(thread, nullptr) 忽略此返回值、SDL 主循环仍继续（窗口静止但不崩），
     * 进程不因此以非 0 退出；失败仅靠上面的 LV_LOG_ERROR 可见。PC 端 setup 失败极罕见。 */
    if (!setup()) {
        return 1;
    }
    do {
        loop();
    } while (*running);
    return 0;
}

int main(int, char **)
{
    return lgfx::Panel_sdl::main(user_func);
}
