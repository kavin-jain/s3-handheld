#include "ui_anim.h"

static void radar_anim_cb(void *var, int32_t v) {
  lv_obj_t *arc = (lv_obj_t *)var;
  lv_arc_set_bg_angles(arc, v, (v + 90) % 360);
}

lv_obj_t *ui_anim_radar_create(lv_obj_t *parent, lv_coord_t size, lv_color_t color) {
  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_set_size(arc, size, size);
  lv_arc_set_bg_angles(arc, 0, 90);
  lv_arc_set_angles(arc, 0, 0); // Hide foreground arc
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB); // No knob
  lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, size / 8, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, 0, LV_PART_MAIN);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, arc);
  lv_anim_set_exec_cb(&a, radar_anim_cb);
  lv_anim_set_values(&a, 0, 360);
  lv_anim_set_time(&a, 1500);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);

  return arc;
}

static void waves_anim_cb(void *var, int32_t v) {
  lv_obj_t *arc = (lv_obj_t *)var;
  lv_obj_set_size(arc, v, v);
  lv_obj_center(arc);
  int32_t opa = 255 - ((v * 255) / 100);
  if (opa < 0) opa = 0;
  lv_obj_set_style_arc_opa(arc, opa, LV_PART_MAIN);
}

lv_obj_t *ui_anim_waves_create(lv_obj_t *parent, lv_coord_t size, lv_color_t color) {
  lv_obj_t *cont = lv_obj_create(parent);
  lv_obj_set_size(cont, size, size);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);

  for (int i = 0; i < 3; i++) {
    lv_obj_t *arc = lv_arc_create(cont);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, 0, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 2, LV_PART_MAIN);
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, waves_anim_cb);
    lv_anim_set_values(&a, 10, size);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_delay(&a, i * 666);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
  }

  return cont;
}

void ui_anim_stop(lv_obj_t *anim_obj) {
  if (anim_obj) {
    lv_anim_del(anim_obj, NULL);
    lv_obj_del(anim_obj);
  }
}
