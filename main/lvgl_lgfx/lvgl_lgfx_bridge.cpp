/*
 * SPDX-FileCopyrightText: 2026 ESP-Pocket2
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl_lgfx_bridge.hpp"

/* 每个 bridge/display 独立的运行时上下文：挂到 lv_display 的 user_data，取代原文件级/函数级 static，
 * 消除 bridge 层的全局 static、运行时状态按 display 隔离（bridge 可重入）。
 * 注：UI 层 ui_app 仍为文件级单实例，多 display 时仅首个 display 建 UI；多屏/多 display 非本设计需求。 */
typedef struct {
    lgfx::LGFX_Device      *lcd;
    lvgl_lgfx_touch_read_fn touch_read;
    lv_display_rotation_t   last_rot;
} lvgl_lgfx_ctx_t;

/* display 删除时释放挂在 user_data 的 ctx（ctx 不泄漏）。注：bridge 另建的 indev 在 lv_display_delete
 * 时仅被 detach、LVGL 不自动删，完整销毁/重建还需另行 lv_indev_delete——当前 demo 单 display 不销毁故不涉及。 */
static void lvgl_lgfx_ctx_delete_cb(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}

static void lvgl_lgfx_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lvgl_lgfx_ctx_t *ctx = (lvgl_lgfx_ctx_t *)lv_display_get_user_data(disp);
    lgfx::LGFX_Device *lcd = ctx->lcd;

    /* G7-B：把 LVGL 逻辑旋转同步到 LGFX（真机=ST7789 MADCTL 硬件旋转；PC=Panel_sdl 坐标映射）。
     * 注：LVGL v9 的 lv_display_set_rotation 只交换内部分辨率、不旋转像素（官方 rotation 文档），
     * 故实际像素旋转由此处 LGFX setRotation 承担，本方案刻意不调用 lv_draw_sw_rotate（省一次软件旋转拷贝）。
     * 方向 (4-rot)&3：LVGL 的触摸 lv_display_rotate_point 与 LovyanGFX setRotation 旋转手性相反——
     * 若显示直接用 rot，则「显示位置」与「LVGL 旋转后的触摸命中区」互为镜像（PC 实测：点看到的按钮点不中）。
     * 取 (4-rot)&3 使显示方向与 LVGL 触摸方向一致（0↔0/90↔270/180↔180/270↔90），配合 read_cb 回传原生坐标
     * 由 LVGL 统一旋转，显示/触摸即对齐。真机/PC 共用同一 bridge，两端方向一致（真机基线另经 esp_lcd_touch
     * flags 校准）。PC 已实测四方向旋转+点击闭环通过（见 spec 第 9 节 PC 旋转测试）。 */
    lv_display_rotation_t rot = lv_display_get_rotation(disp);
    if (rot != ctx->last_rot) {
        ctx->last_rot = rot;
        lcd->setRotation((uint_fast8_t)((4 - (uint_fast8_t)rot) & 3));
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
    lvgl_lgfx_ctx_t *ctx =
        (lvgl_lgfx_ctx_t *)lv_display_get_user_data(lv_indev_get_display(indev));
    int32_t x = 0, y = 0;
    if (ctx != NULL && ctx->touch_read != NULL && ctx->touch_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

lv_display_t *lvgl_lgfx_bridge_create(const lvgl_lgfx_bridge_cfg_t *cfg)
{
    /* 显式判空并返回（始终生效，不依赖 LV_USE_ASSERT_NULL）；调用方须保证三者非空 */
    if (cfg == NULL || cfg->lcd == NULL || cfg->buf1 == NULL) {
        LV_LOG_ERROR("lvgl_lgfx_bridge_create: cfg/lcd/buf1 must not be NULL");
        return NULL;
    }

    lv_display_t *disp = lv_display_create(UI_APP_HOR_RES, UI_APP_VER_RES);
    if (disp == NULL) {
        return NULL;
    }

    lvgl_lgfx_ctx_t *ctx = (lvgl_lgfx_ctx_t *)lv_malloc(sizeof(lvgl_lgfx_ctx_t));
    if (ctx == NULL) {
        lv_display_delete(disp);
        LV_LOG_ERROR("lvgl_lgfx_bridge_create: ctx alloc failed");
        return NULL;
    }
    ctx->lcd        = cfg->lcd;
    ctx->touch_read = cfg->touch_read;
    ctx->last_rot   = LV_DISPLAY_ROTATION_0;

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, ctx);
    lv_display_add_event_cb(disp, lvgl_lgfx_ctx_delete_cb, LV_EVENT_DELETE, ctx);
    lv_display_set_flush_cb(disp, lvgl_lgfx_flush_cb);
    lv_display_set_buffers(disp, cfg->buf1, cfg->buf2, cfg->buf_size_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

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
