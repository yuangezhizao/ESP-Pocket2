/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 改编自官方 lvgl_demo_ui.c（LVGL scatter chart），适配 v9。
 * 首版省略官方 v8 的逐点渐变上色回调（v9 draw-task 事件模型），仅用单色散点确保跑通。
 */
#include "lvgl.h"
#include "lvgl_qemu_rgb.h"

static void qemu_rgb_add_data(lv_timer_t *timer)
{
    lv_obj_t *chart = (lv_obj_t *)lv_timer_get_user_data(timer);
    lv_chart_series_t *ser = lv_chart_get_series_next(chart, NULL);
    lv_chart_set_next_value2(chart, ser, lv_rand(0, 200), lv_rand(0, 1000));
}

void qemu_rgb_lvgl_demo_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    /* 顶部提示文字，便于确认渲染成功 */
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, LV_SYMBOL_BELL " Hello Espressif & LVGL on QEMU RGB " LV_SYMBOL_BELL);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    /* 散点图 */
    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 200, 150);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_line_width(chart, 0, LV_PART_ITEMS); /* 去掉连线，仅散点 */

    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, 200);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    lv_chart_set_point_count(chart, 50);

    lv_chart_series_t *ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 50; i++) {
        lv_chart_set_next_value2(chart, ser, lv_rand(0, 200), lv_rand(0, 1000));
    }

    lv_timer_create(qemu_rgb_add_data, 200, chart);
}
