/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 可移植 UI 层实现：由 main/lvgl/hp28008.c 的 app_main_display() 抽离而来，去除对 RTC 等硬件的直接依赖，时钟数据源改为回调注入。仅依赖 LVGL，可在真机/QEMU/PC(SDL) 复用。
 */
#include "ui_app.h"

/* LVGL image declare（图片为可移植 C array，真机由构建系统生成、PC 由 pc_simulator/CMakeLists.txt 构建时动态生成到 build 目录） */
LV_IMG_DECLARE(esp_logo)

static lv_obj_t *s_clock_label = NULL;
static ui_clock_source_fn s_clock_src = NULL;
static lv_timer_t *s_clock_timer = NULL;

static void ui_app_rotate_btn_cb(lv_event_t *e)
{
    /* 从被点击按钮所属 display 取旋转对象，兼容多 display（不依赖 default display） */
    lv_obj_t *btn = lv_event_get_target_obj(e);
    lv_display_t *disp = lv_obj_get_display(btn);
    lv_disp_rotation_t rotation = lv_disp_get_rotation(disp);
    rotation++;
    if (rotation > LV_DISPLAY_ROTATION_270)
    {
        rotation = LV_DISPLAY_ROTATION_0;
    }
    lv_disp_set_rotation(disp, rotation);
}

static void ui_app_clock_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    /* s_clock_label 由 ui_app_create 一次性设置，之后不会归零。
     * 单画面嵌入式用途下屏幕不会被销毁，故当前安全。 */
    const char *txt = (s_clock_src != NULL) ? s_clock_src() : NULL;
    lv_label_set_text(s_clock_label, (txt != NULL) ? txt : "Loading time...");
}

void ui_app_create(lv_obj_t *scr, ui_clock_source_fn clock_src)
{
    /* 运行时硬防重复调用：即使 LVGL assert 被关闭也生效，避免重复创建 timer 导致旧 timer 悬挂（handle 丢失、无法删除、仍持续触发并可能访问旧对象）。若将来需要销毁重建，见 docs/superpowers 中的方案 2(ui_app_destroy)/方案 3(ui_app_t 实例化) */
    if (s_clock_label != NULL)
    {
        LV_LOG_WARN("ui_app_create called more than once, ignored");
        return;
    }
    s_clock_src = clock_src;

    /* Create image */
    lv_obj_t *img_logo = lv_img_create(scr);
    lv_img_set_src(img_logo, &esp_logo);
    lv_obj_align(img_logo, LV_ALIGN_TOP_MID, 0, 20);

    /* Label（宽度取满屏配合居中；用 lv_pct 保持可移植） */
    lv_obj_t *label = lv_label_create(scr);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
#if LVGL_VERSION_MAJOR == 8
    lv_label_set_recolor(label, true);
    lv_label_set_text(label, "#FF0000 " LV_SYMBOL_BELL " Hello world Espressif and LVGL " LV_SYMBOL_BELL "#\n#FF9400 " LV_SYMBOL_WARNING " For simplier initialization, use BSP " LV_SYMBOL_WARNING " #");
#else
    lv_label_set_text(label, LV_SYMBOL_BELL " Hello world Espressif and LVGL " LV_SYMBOL_BELL "\n " LV_SYMBOL_WARNING " For simplier initialization, use BSP " LV_SYMBOL_WARNING);
#endif
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);

    /* Clock Label */
    s_clock_label = lv_label_create(scr);
    lv_obj_set_width(s_clock_label, lv_pct(100));
    lv_obj_set_style_text_align(s_clock_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_clock_label, "Loading time...");
    lv_obj_align(s_clock_label, LV_ALIGN_CENTER, 0, 80);

    /* Start LVGL clock timer (for UI updates only) */
    s_clock_timer = lv_timer_create(ui_app_clock_timer_cb, 1000, NULL);

    /* Button */
    lv_obj_t *btn = lv_btn_create(scr);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, "Rotate screen");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(btn, ui_app_rotate_btn_cb, LV_EVENT_CLICKED, NULL);
}
