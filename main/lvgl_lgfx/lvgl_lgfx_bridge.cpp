/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl_lgfx_bridge.hpp"

static lvgl_lgfx_touch_read_fn s_touch_read = NULL;

static void lvgl_lgfx_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lgfx::LGFX_Device *lcd = (lgfx::LGFX_Device *)lv_display_get_user_data(disp);

    /* G7-B：把 LVGL 逻辑旋转同步到 LGFX（真机=ST7789 MADCTL 硬件旋转；PC=Panel_sdl 坐标映射）。
     * 注：LVGL v9 的 lv_display_set_rotation 只交换内部分辨率、不旋转像素（官方 rotation 文档），
     * 故实际像素旋转由此处 LGFX setRotation 承担，本方案刻意不调用 lv_draw_sw_rotate（省一次软件旋转拷贝）。
     * lv_display_rotation_t 的 0/90/180/270 枚举值即 0..3，与 LGFX setRotation 的 0..3 对应。 */
    static lv_display_rotation_t s_last_rot = LV_DISPLAY_ROTATION_0;
    lv_display_rotation_t rot = lv_display_get_rotation(disp);
    if (rot != s_last_rot) {
        s_last_rot = rot;
        lcd->setRotation((uint_fast8_t)rot);
    }

    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    lcd->startWrite();
    lcd->pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
    lcd->endWrite();
    lv_display_flush_ready(disp);
}

static void lvgl_lgfx_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);
    int32_t x = 0, y = 0;
    if (s_touch_read != NULL && s_touch_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

lv_display_t *lvgl_lgfx_bridge_create(const lvgl_lgfx_bridge_cfg_t *cfg)
{
    LV_ASSERT_NULL(cfg);
    LV_ASSERT_NULL(cfg->lcd);
    LV_ASSERT_NULL(cfg->buf1);

    lv_display_t *disp = lv_display_create(UI_APP_HOR_RES, UI_APP_VER_RES);
    if (disp == NULL) {
        return NULL;
    }
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, cfg->lcd);
    lv_display_set_flush_cb(disp, lvgl_lgfx_flush_cb);
    lv_display_set_buffers(disp, cfg->buf1, cfg->buf2, cfg->buf_size_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    s_touch_read = cfg->touch_read;
    lv_indev_t *indev = lv_indev_create();
    if (indev != NULL) {
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(indev, disp);
        lv_indev_set_read_cb(indev, lvgl_lgfx_read_cb);
    } else {
        LV_LOG_WARN("lv_indev_create failed; display works but input is disabled");
    }

    ui_app_create(lv_display_get_screen_active(disp), cfg->clock_src);
    return disp;
}
