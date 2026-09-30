#pragma once
#include <lvgl.h>

enum MascotId : uint8_t { MASCOT_L, MASCOT_LIGHT, MASCOT_RYUK, MASCOT_MISA,
                          MASCOT_NFC_CARD, MASCOT_IR_BEAM, MASCOT_NOTEBOOK };

// Plays a mascot/object sprite as an lv_animimg under `parent`.
// loop=true:  infinite idle loop; caller's screen owns its lifetime as usual.
// loop=false: plays once, then self-deletes via lv_obj_del_delayed (safe even
//             if the parent screen is torn down first — LVGL cancels pending
//             per-object animations, incl. this one, on lv_obj_del).
lv_obj_t *mascot_play(lv_obj_t *parent, MascotId who, lv_align_t align,
                      bool loop, uint32_t frame_ms = 220);
