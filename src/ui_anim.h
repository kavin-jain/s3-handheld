#pragma once
#include <lvgl.h>

// Creates a radar sweep animation for Sub-GHz/WiFi scanning.
lv_obj_t *ui_anim_radar_create(lv_obj_t *parent, lv_coord_t size, lv_color_t color);

// Creates expanding wave animations for NFC reading / IR blasting.
lv_obj_t *ui_anim_waves_create(lv_obj_t *parent, lv_coord_t size, lv_color_t color);

// Stop and destroy animation instances
void ui_anim_stop(lv_obj_t *anim_obj);
