#pragma once
#include <Arduino.h>
#include <lvgl.h>

#define C_BG        0x03070b
#define C_PANEL     0x0a141a
#define C_TXT       0xe0ffe0
#define C_SUB       0x88cc88
#define C_MUTE      0x446644
#define C_GREEN     0x44ff44
#define C_GREEN_SFT 0x114411
#define C_AMBER     0xffbb00
#define C_RED       0xff4444
#define C_CYAN      0x00ffff
#define C_CYAN_BG   0x08222a

enum Kind : uint8_t { K_OK, K_SOON, K_ATK, K_DEF };

struct Tool {
  const char *code;
  const char *name;
  const char *sub;
  Kind kind;
  void (*builder)(lv_obj_t *box);
};

struct Category {
  const char *icon;
  const char *name;
  const char *tag;
  const Tool *tools;
  uint8_t n;
};

enum ScreenT : uint8_t { SCR_HOME, SCR_AROUND, SCR_CATEGORY, SCR_TOOL, SCR_SETTINGS,
                         SCR_EDIT_BRIGHT, SCR_EDIT_POWER };

extern void (*g_action_cb)();

// UI Builder primitives
lv_obj_t *new_screen(const char *title);
lv_obj_t *content_box(lv_obj_t *scr);
lv_obj_t *panel(lv_obj_t *parent);
lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t hex);
lv_obj_t *make_list(lv_obj_t *parent);
void add_row(lv_obj_t *list, const char *chip, uint32_t chip_fg, const char *title, const char *sub, const char *icon, int action, int p1, int p2);
void load_screen(lv_obj_t *scr);
void section(lv_obj_t *scr, const char *txt);
void nav_push(ScreenT t, int cat, int tool);
void nav_pop();
int nav_code(int t, int cat, int tool);
