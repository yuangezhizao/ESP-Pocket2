/**
 * @file lv_conf.h
 * LVGL PC simulator configuration for pc_simulator.
 * Settings aligned with the firmware build (LVGL v9.5, RGB565 color).
 */

/* clang-format off */
#if 1 /* Enable this file */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16

/*====================
    MEMORY SETTINGS
 *====================*/
#define LV_MEM_SIZE (256U * 1024U)

/*====================
   HAL SETTINGS
 *====================*/
#define LV_USE_SDL  1
/* The SDL sw backend (lv_sdl_sw.c) only applies lv_draw_sw_rotate() in flush_cb when
 * LV_SDL_RENDER_MODE == PARTIAL. The default DIRECT mode skips rotation, causing
 * lv_display_set_rotation() to produce a skewed image. Set PARTIAL so rotation works. */
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL

/*====================
 * FONT USAGE
 *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
 * WIDGET USAGE
 *====================*/
#define LV_USE_LABEL  1
#define LV_USE_BUTTON 1
#define LV_USE_IMAGE  1

/*====================
 * EXTRA COMPONENTS
 *====================*/
#define LV_USE_OBSERVER 1

/*====================
 * DEMOS / EXAMPLES
 *====================*/
#define LV_USE_DEMOS    0
#define LV_USE_EXAMPLES 0

/*====================
 * LOGGING
 *====================*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#endif /* LV_CONF_H */
#endif /* Enable this file */
