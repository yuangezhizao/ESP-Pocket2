/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 改编自官方 lvgl_demo_ui.c（LVGL scatter chart），适配 v9。
 * 逐点渐变已按 v9 draw-task 还原；坐标轴刻度因 v9 移除 chart tick 未补。
 */
#include "lvgl.h"
#include "lvgl_qemu_rgb.h"

static void qemu_rgb_draw_event_cb(lv_event_t *e)
{
    lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    if (base_dsc->part != LV_PART_INDICATOR) {
        return;
    }
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_chart_series_t *ser = lv_chart_get_series_next(obj, NULL);
    lv_draw_fill_dsc_t *fill_dsc = lv_draw_task_get_fill_dsc(draw_task);
    if (fill_dsc == NULL) {
        return;
    }
    uint32_t cnt = lv_chart_get_point_count(obj);
    /* 旧点更透明 */
    fill_dsc->opa = (lv_opa_t)((LV_OPA_COVER * base_dsc->id2) / (cnt - 1));
    /* 小值偏蓝、大值偏红 */
    int32_t *x_array = lv_chart_get_series_x_array(obj, ser);
    int32_t *y_array = lv_chart_get_series_y_array(obj, ser);
    uint32_t start_point = lv_chart_get_x_start_point(obj, ser);
    uint32_t p_act = (start_point + base_dsc->id2) % cnt;
    lv_opa_t x_opa = (lv_opa_t)((x_array[p_act] * LV_OPA_50) / 200);
    lv_opa_t y_opa = (lv_opa_t)((y_array[p_act] * LV_OPA_50) / 1000);
    fill_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_RED),
                                   lv_palette_main(LV_PALETTE_BLUE),
                                   x_opa + y_opa);
}

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

    /* 诊断：屏幕底部显示本次实际生效配置，便于确认 buffer 落点（保留入库） */
    lv_obj_t *diag_label = lv_label_create(scr);
    lv_label_set_text(diag_label, qemu_rgb_diag_str());
    lv_obj_align(diag_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* 散点图 */
    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 200, 150);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_line_width(chart, 0, LV_PART_ITEMS); /* 去掉连线，仅散点 */
    lv_obj_add_event_cb(chart, qemu_rgb_draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_flag(chart, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

    lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, 200);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    lv_chart_set_point_count(chart, 50);

    lv_chart_series_t *ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 50; i++) {
        lv_chart_set_next_value2(chart, ser, lv_rand(0, 200), lv_rand(0, 1000));
    }

    lv_timer_create(qemu_rgb_add_data, 100, chart);  /* 200 -> 100，对齐官方 */
}
