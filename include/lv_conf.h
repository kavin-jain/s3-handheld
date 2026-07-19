/* Minimal LVGL v8.3 config for the ESP32-S3 handheld.
 * Only the settings we deliberately change are here; every other option
 * falls back to LVGL's built-in default (see lv_conf_internal.h). */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* 16-bit colour, matches ILI9341. We byte-swap in the flush callback via
 * tft.setSwapBytes(true), so leave LV_COLOR_16_SWAP at its default (0). */
#define LV_COLOR_DEPTH 16

/* LVGL's internal memory pool (widgets, styles). 64 KB is plenty for our UI. */
#define LV_MEM_SIZE (64U * 1024U)

/* Drive LVGL's clock from the ESP timer. esp_timer.h is C-safe (Arduino.h is
 * NOT — including it here would break LVGL's C files). */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "esp_timer.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)(esp_timer_get_time() / 1000))

/* Fonts we use on the home screen (14 is on by default; enable the rest). */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1

/* UNSCII 8 — a blocky bitmap "terminal" font for hacker-style data readouts
 * (frequencies, hex, IDs). Also duplicated as a build flag so the LVGL
 * library's own units see it (same reason as the montserrat flags). */
#define LV_FONT_UNSCII_8 1

#define LV_USE_LOG 0

#endif /* LV_CONF_H */
