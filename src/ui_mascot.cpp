#include "ui_mascot.h"
#include "sprites.h"

static void mascot_set(MascotId who, const lv_img_dsc_t *const **frames, uint8_t *count) {
  switch (who) {
    case MASCOT_L:        *frames = l_frames;        *count = l_frame_count;        break;
    case MASCOT_LIGHT:    *frames = light_frames;    *count = light_frame_count;    break;
    case MASCOT_RYUK:     *frames = ryuk_frames;     *count = ryuk_frame_count;     break;
    case MASCOT_MISA:     *frames = misa_frames;     *count = misa_frame_count;     break;
    case MASCOT_NFC_CARD: *frames = nfc_card_frames; *count = nfc_card_frame_count; break;
    case MASCOT_IR_BEAM:  *frames = ir_beam_frames;  *count = ir_beam_frame_count;  break;
    case MASCOT_NOTEBOOK: *frames = notebook_frames; *count = notebook_frame_count; break;
    default:              *frames = nullptr;         *count = 0;                    break;
  }
}

lv_obj_t *mascot_play(lv_obj_t *parent, MascotId who, lv_align_t align,
                      bool loop, uint32_t frame_ms) {
  const lv_img_dsc_t *const *frames; uint8_t count;
  mascot_set(who, &frames, &count);
  if (!frames || !count) return nullptr;

  lv_obj_t *obj = lv_animimg_create(parent);
  // Every mascot call site is inside a flex-COLUMN container (panel()/content_box()/
  // screens all set LV_FLEX_FLOW_COLUMN) — without this, the flex layout engine
  // overrides lv_obj_align() below on its next layout pass, and objects added to an
  // already-full screen (the one-shot reactions below) get squeezed to zero space.
  lv_obj_add_flag(obj, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_animimg_set_src(obj, (const void **)frames, count);
  lv_animimg_set_duration(obj, frame_ms * count);
  lv_animimg_set_repeat_count(obj, loop ? LV_ANIM_REPEAT_INFINITE : 1);
  lv_animimg_start(obj);
  lv_obj_align(obj, align, 0, 0);
  if (!loop) lv_obj_del_delayed(obj, frame_ms * count);
  return obj;
}
