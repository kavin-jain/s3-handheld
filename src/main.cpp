// ESP32-S3 handheld — UI shell, milestone 2.
// Black/green "terminal" theme, rotary-encoder + 3-button navigation, a
// Flipper-style menu tree, and a real idle power manager (dim -> light sleep).
// Radio/NFC/etc. data is still static demo — live data is a later milestone.
//
// Controls:  rotate = move  ·  click = select  ·  BACK/HOME/ACTION buttons.
// Renders through TFT_eSPI (confirmed working). Pins come from include/pins.h.

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "pins.h"
#include "storage.h"
#include "radio_cc1101.h"
#include "subghz_classify.h"
#include "subghz_replay.h"
#include "wmbus.h"
#include "amiibo.h"
#include "emv.h"
#include "mifare.h"
#include "nfc_pn532.h"
#include "nfc_keys.h"
#include "ndef.h"
#include "transit.h"
#include "ibutton.h"
#include "ir_remote.h"
#include "irdb.h"
#include "wifi_scan.h"
#include "wifi_fmt.h"
#include "ble_scan.h"
#include "ble_track.h"
#include "gatt_uuid.h"
#include "badusb.h"
#include "deauth_detect.h"
#include "nrf24_radio.h"
#include "nrf_band.h"
#include "mousejack.h"
#include "karma.h"
#include "evilportal.h"
#include "handshake.h"
#include "wardrive.h"
#include "buspirate.h"
#include "fwdump.h"
#include "gpio_util.h"
#include "droneid.h"
#include "skimmer.h"
#include "audiobug.h"
#include "ssdp.h"
#include "hackscreen.h"
#include "hid_keymap.h"
#include "ical.h"
#include "tasks.h"
#include "camera_detect.h"
#include "tvbgone.h"
#include "wof.h"
#include "usage_fmt.h"
#include "df_logic.h"
#include "csi_motion.h"
#include "ui_edit.h"
#include "espnow_mesh.h"

// ---------------------------------------------------------------- power knobs
#define DIM_AFTER_MS     20000    // active -> dim
#define SLEEP_AFTER_MS   35000    // active -> sleep (dim + this gap)
#define DIM_DUTY         40       // backlight duty in DIM (0..255)
#define ENABLE_LIGHT_SLEEP 1      // 0 while USB-debugging (light sleep drops CDC)
#define ENC_STEPS_PER_DETENT 4    // EC11 quadrature transitions per click; tune

// ---------------------------------------------------------------- palette (RGB)
#define C_BG        0x05070a
#define C_CARD      0x0c1410
#define C_CARD_FOC  0x11291c
#define C_LINE      0x1a2a20
#define C_CHIP      0x0f1d16
#define C_GREEN     0x39ff14   // phosphor accent
#define C_GREEN_SFT 0x57e389
#define C_TXT       0xd7ffe6
#define C_SUB       0x6f8f7c
#define C_MUTE      0x46584e
#define C_RED       0xff5c5c
#define C_RED_BG    0x2a0f0f
#define C_AMBER     0xffb454
#define C_CYAN      0x54e6ff
#define C_CYAN_BG   0x08222a

// ---------------------------------------------------------------- display glue
static TFT_eSPI tft = TFT_eSPI();
static const uint16_t SCR_W = 320, SCR_H = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCR_W * 40];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  uint32_t w = area->x2 - area->x1 + 1, h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&px->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

// ---------------------------------------------------------------- feature data
enum Kind : uint8_t { K_OK, K_SOON, K_ATK, K_DEF };   // badge flavor

struct Tool { const char *code; const char *name; const char *sub; Kind kind; };
struct Category { const char *icon; const char *name; const char *tag; const Tool *tools; uint8_t n; };

static const Tool T_SUBGHZ[] = {
  {"SG",  "Frequency finder", "sweep the band, name the signal", K_OK},
  {"CR",  "Capture & replay", "fixed-code gates & remotes",      K_ATK},
  {"BF",  "Gate brute-force", "De Bruijn - your own gate",       K_ATK},
  {"TS",  "Tesla charge port","315 MHz open",                    K_OK},
  {"433", "ISM decoder",      "weather/doorbell/TPMS (rtl_433)", K_OK},
  {"WMB", "wM-Bus meter",     "read utility meters, 868 MHz",    K_OK},
};
static const Tool T_NFC[] = {
  {"NR",  "Read / clone",     "Mifare, NTAG & more",             K_OK},
  {"MF",  "Mifare crack",     "dictionary keys",                 K_ATK},
  {"EMV", "Bank card read",   "public data only",                K_OK},
  {"TR",  "Transit card",     "metro balance & history",         K_OK},
  {"ND",  "Write NDEF tag",   "URL / WiFi / vCard",              K_OK},
  {"AM",  "Amiibo clone",     "to NTAG215",                      K_OK},
  {"IB",  "iButton key",      "1-Wire Dallas",                   K_OK},
};
static const Tool T_IR[] = {
  {"UR",  "Universal remote", "TV + A/C brand database",         K_OK},
  {"LB",  "Learn & blast",    "capture any remote",              K_OK},
  {"TVB", "TV-B-Gone",        "shut off any TV",                 K_OK},
};
static const Tool T_WIFI[] = {
  {"SC",  "Scan / recon",     "who is here",                     K_OK},
  {"DA",  "Deauth",           "kick a client",                   K_ATK},
  {"EP",  "Evil Portal",      "captive login clone",             K_ATK},
  {"HS",  "Handshake / PMKID","capture to SD",                   K_ATK},
  {"WD",  "Wardrive",         "log nets + GPS to SD",            K_OK},
  {"KM",  "Karma / MANA",     "auto-associate probes",           K_ATK},
};
static const Tool T_BLE[] = {
  {"BS",  "Scan / recon",     "devices around you",              K_OK},
  {"GT",  "GATT explore",     "services & characteristics",      K_OK},
  {"TK",  "Tracker hunt",     "AirTag / Tile near me",           K_DEF},
  {"WF",  "Wall of Flipper",  "spot other hacking gear",         K_DEF},
};
static const Tool T_NRF[] = {
  {"MJ",  "Mousejack",        "wireless kbd/mouse inject",       K_ATK},
  {"KS",  "Keyboard sniff",   "log 2.4 GHz keystrokes",          K_ATK},
  {"BN",  "Band scanner",     "2.4 GHz activity map",            K_OK},
};
static const Tool T_USB[] = {
  {"DK",  "DuckyScript",      "run payload from SD",             K_ATK},
  {"HID", "HID attacks",      "keystroke injection",             K_ATK},
};
static const Tool T_SPY[] = {
  {"HC",  "Hidden camera",    "wireless lens finder",            K_DEF},
  {"ME",  "Tracker on me?",   "GPS/BLE bug sweep",               K_DEF},
  {"AB",  "Audio bug sweep",  "RF listening devices",            K_DEF},
  {"DD",  "Deauth detector",  "is someone jamming me?",          K_DEF},
  {"SK",  "Skimmer detector", "rogue card readers",              K_DEF},
  {"DR",  "Drone spotter",    "Remote-ID + pilot location",      K_DEF},
};
static const Tool T_SENSE[] = {
  {"WW",  "See through wall",  "WiFi CSI motion & breathing",    K_OK},
  {"DF",  "Direction finder",  "fox-hunt a signal",             K_OK},
};
static const Tool T_FUN[] = {
  {"TVB", "TV-B-Gone",        "turn everything off",             K_OK},
  {"RR",  "Rickroll tag",     "NFC that opens the song",         K_OK},
  {"HK",  "Hacker screen",    "fake spy-movie hack",             K_OK},
  {"GG",  "USB gag",          "harmless keyboard prank",         K_OK},
  {"CST", "Cast crasher",     "poke nearby TVs / Rokus",         K_OK},
};
static const Tool T_TOOLS[] = {
  {"BP",  "Bus Pirate",       "sniff I2C/SPI/UART/JTAG",         K_OK},
  {"FD",  "Firmware dump",    "read & analyse flash",            K_OK},
  {"GP",  "GPIO play",        "toggle & read pins",              K_OK},
};
static const Tool T_COMMS[] = {
  {"EN",  "ESP-NOW mesh",     "router-free messaging",           K_OK},
  {"UH",  "USB host",         "read a flash drive",              K_OK},
  {"MT",  "Meshtastic",       "needs LoRa add-on",               K_SOON},
};
static const Tool T_ME[] = {
  {"CL",  "Claude usage",     "5h & weekly meter",               K_SOON},
  {"CAL", "Calendar",         "from your phone",                 K_SOON},
  {"TSK", "Tasks",            "from your phone",                 K_SOON},
  {"FID", "Security key",     "FIDO2 / U2F",                     K_OK},
  {"LNK", "Phone + web",      "companion & dashboard",           K_SOON},
};

#define CAT(icon, name, tag, arr) {icon, name, tag, arr, (uint8_t)(sizeof(arr)/sizeof(arr[0]))}
static const Category CATS[] = {
  CAT("RF",  "Sub-GHz",         "read, replay, decode 300-900 MHz", T_SUBGHZ),
  CAT("NFC", "RFID / NFC",      "read, clone, emulate 13.56 MHz",   T_NFC),
  CAT("IR",  "Infrared",        "remote for everything",            T_IR),
  CAT("WiFi","WiFi",            "recon & attack",                   T_WIFI),
  CAT("BLE", "Bluetooth",       "scan & explore",                   T_BLE),
  CAT("2.4", "NRF24 / 2.4GHz",  "wireless keyboards & mice",        T_NRF),
  CAT("USB", "BadUSB / HID",    "be a keyboard",                    T_USB),
  CAT("EYE", "Am I safe?",      "counter-surveillance sweep",       T_SPY),
  CAT("CSI", "See invisible",   "through-wall & direction",         T_SENSE),
  CAT("FUN", "Pranks",          "harmless mischief",                T_FUN),
  CAT("BUS", "Tools / Bench",   "the hacker toolbox",               T_TOOLS),
  CAT("NET", "Comms / Off-grid","talk without a network",           T_COMMS),
  CAT("ME",  "Me",              "usage, calendar, keys",            T_ME),
};
static const uint8_t N_CATS = sizeof(CATS) / sizeof(CATS[0]);

// ---------------------------------------------------------------- nav + input
enum ScreenT : uint8_t { SCR_HOME, SCR_AROUND, SCR_CATEGORY, SCR_TOOL, SCR_SETTINGS, SCR_EDIT_BRIGHT };
struct NavEntry { ScreenT t; int8_t cat; int8_t tool; };
static NavEntry nav_stack[8];
static uint8_t  nav_depth = 0;

static lv_group_t *g_group;
static lv_indev_t *g_enc_indev;

static Adafruit_MCP23X17 mcp;
static bool mcp_ok = false;
static volatile int32_t enc_accum = 0;         // raw quadrature counts (ISR)
static volatile uint8_t enc_prev = 0;
static volatile bool    g_enc_pressed = false; // encoder click (from MCP)
static uint8_t btn_prev = 0;

// ---------------------------------------------------------------- power mgmt
enum PM : uint8_t { PM_ACTIVE, PM_DIM, PM_SLEEP };
static PM       pm_state = PM_ACTIVE;
static uint32_t last_input_ms = 0;
static uint8_t  bl_user_duty = 255;            // full brightness (Settings later)

static void bl_write(uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BL_PWM, duty);
#else
  ledcWrite(BL_LEDC_CH, duty);
#endif
}

// ---- encoder value-editing (Settings) ----
// When g_edit_val is non-null, encoder rotation changes *g_edit_val instead of
// moving list focus. Cleared on every screen change (render_top).
static int  g_bright_pct = 100;
static int *g_edit_val = nullptr;
static int  g_edit_min, g_edit_max, g_edit_step;
static void (*g_edit_cb)(int) = nullptr;
static lv_obj_t *g_edit_label = nullptr;

static void apply_brightness(int pct) {
  bl_user_duty = (uint8_t)(pct * 255 / 100);
  bl_write(bl_user_duty);
}

static void pm_wake() {                        // -> ACTIVE (called on any input)
  last_input_ms = millis();
  if (pm_state != PM_ACTIVE) {
    setCpuFrequencyMhz(240);
    bl_write(bl_user_duty);
    pm_state = PM_ACTIVE;
  }
}

static void pm_tick() {
  uint32_t idle = millis() - last_input_ms;
  if (pm_state == PM_ACTIVE && idle > DIM_AFTER_MS) {
    bl_write(DIM_DUTY);
    setCpuFrequencyMhz(80);
    pm_state = PM_DIM;
    Serial.println("[pm] dim");
  } else if (pm_state == PM_DIM && idle > SLEEP_AFTER_MS) {
    Serial.println("[pm] sleep (screen off; turn/press to wake)");
    Serial.flush();
    bl_write(0);
    pm_state = PM_SLEEP;
#if ENABLE_LIGHT_SLEEP
    gpio_wakeup_enable((gpio_num_t)PIN_ENC_A, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)PIN_ENC_B, GPIO_INTR_LOW_LEVEL);
    if (mcp_ok) gpio_wakeup_enable((gpio_num_t)PIN_MCP_INT, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    esp_light_sleep_start();                   // blocks here until an input edge
    gpio_wakeup_disable((gpio_num_t)PIN_ENC_A);
    gpio_wakeup_disable((gpio_num_t)PIN_ENC_B);
    if (mcp_ok) gpio_wakeup_disable((gpio_num_t)PIN_MCP_INT);
#endif
    pm_wake();
  }
}

// ---------------------------------------------------------------- encoder ISR
// Full-step quadrature decode: ±1 per valid transition, summed in enc_accum.
static const int8_t QDEC[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
static void IRAM_ATTR enc_isr() {
  uint8_t s = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  enc_accum += QDEC[((enc_prev << 2) | s) & 0x0f];
  enc_prev = s;
}

static void pm_wake();
static void enc_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static int32_t consumed = 0;
  int32_t diff  = enc_accum - consumed;
  int32_t steps = diff / ENC_STEPS_PER_DETENT;
  if (steps != 0) { consumed += steps * ENC_STEPS_PER_DETENT; pm_wake(); }
  if (g_edit_val && steps) {                    // edit mode: rotation changes a value
    int nv = edit_apply(*g_edit_val, steps, g_edit_min, g_edit_max, g_edit_step);
    if (nv != *g_edit_val) {
      *g_edit_val = nv;
      if (g_edit_cb) g_edit_cb(nv);
      if (g_edit_label) lv_label_set_text_fmt(g_edit_label, "%d%%", nv);
    }
    data->enc_diff = 0;                          // don't move list focus while editing
  } else {
    data->enc_diff = steps;
  }
  data->state = g_enc_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Poll the expander's 4 button/click inputs (active-low) every ~30 ms.
static void nav_push(ScreenT t, int cat, int tool);
static void nav_pop();
static void nav_home();
static void on_action();

static void poll_buttons(lv_timer_t *) {
  if (!mcp_ok) return;
  uint8_t pressed = (~mcp.readGPIOA()) & 0x0f;   // 1 = pressed
  uint8_t edges   = pressed & ~btn_prev;         // rising (newly pressed)
  btn_prev = pressed;
  g_enc_pressed = pressed & (1 << MCP_ENC_SW);
  if (edges) pm_wake();
  if      (edges & (1 << MCP_BTN_BACK))   nav_pop();
  else if (edges & (1 << MCP_BTN_HOME))   nav_home();
  else if (edges & (1 << MCP_BTN_ACTION)) on_action();
}

// ---------------------------------------------------------------- UI helpers
static lv_style_t st_screen, st_item, st_item_foc, st_chip;

static lv_obj_t *make_label(lv_obj_t *p, const char *txt, const lv_font_t *f, uint32_t col) {
  lv_obj_t *l = lv_label_create(p);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  return l;
}

static lv_obj_t *plain(lv_obj_t *p) {           // transparent layout box
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

static void kind_colors(Kind k, uint32_t &fg, uint32_t &bg) {
  switch (k) {
    case K_ATK:  fg = C_RED;   bg = C_RED_BG;  break;
    case K_DEF:  fg = C_CYAN;  bg = C_CYAN_BG; break;
    case K_SOON: fg = C_MUTE;  bg = C_LINE;    break;
    default:     fg = C_GREEN; bg = C_CHIP;    break;
  }
}
static const char *kind_text(Kind k) {
  switch (k) { case K_ATK: return "attack"; case K_DEF: return "detect";
               case K_SOON: return "soon";  default: return "ready"; }
}

// status bar across the top of every screen
static void build_statusbar(lv_obj_t *scr, const char *title) {
  lv_obj_t *bar = plain(scr);
  lv_obj_set_size(bar, lv_pct(100), 24);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(bar, 10, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, 8, 0);
  make_label(bar, title, &lv_font_unscii_8, C_GREEN);
  lv_obj_t *sp = plain(bar); lv_obj_set_flex_grow(sp, 1); lv_obj_set_height(sp, 1);
  make_label(bar, LV_SYMBOL_GPS, &lv_font_montserrat_14, C_MUTE);
  make_label(bar, LV_SYMBOL_SD_CARD, &lv_font_montserrat_14, storage_ready() ? C_GREEN : C_MUTE);
  make_label(bar, LV_SYMBOL_BATTERY_FULL " 82%", &lv_font_unscii_8, C_GREEN_SFT);
}

// a focusable row: [chip] title / sub .......... [chevron], click -> nav dest
static void item_clicked_cb(lv_event_t *e) {
  intptr_t code = (intptr_t)lv_event_get_user_data(e);
  nav_push((ScreenT)((code >> 16) & 0xFF), (code >> 8) & 0xFF, code & 0xFF);
}
static void focus_scroll_cb(lv_event_t *e) {
  lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_ON);
}
static intptr_t nav_code(ScreenT t, int cat, int tool) {
  return ((intptr_t)t << 16) | ((cat & 0xFF) << 8) | (tool & 0xFF);
}

static lv_obj_t *make_list(lv_obj_t *scr) {
  lv_obj_t *list = plain(scr);
  lv_obj_set_width(list, lv_pct(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_style_pad_hor(list, 9, 0);
  lv_obj_set_style_pad_ver(list, 8, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  return list;
}

static void add_row(lv_obj_t *list, const char *chip, uint32_t chip_fg,
                    const char *title, const char *sub,
                    const char *badge, uint32_t badge_fg, uint32_t badge_bg,
                    intptr_t dest) {
  lv_obj_t *row = lv_btn_create(list);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, lv_pct(100), 50);
  lv_obj_add_style(row, &st_item, 0);
  lv_obj_add_style(row, &st_item_foc, LV_STATE_FOCUSED);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(row, 9, 0);
  lv_obj_set_style_pad_column(row, 10, 0);

  lv_obj_t *chipb = lv_obj_create(row);
  lv_obj_remove_style_all(chipb);
  lv_obj_add_style(chipb, &st_chip, 0);
  lv_obj_set_size(chipb, 38, 32);
  lv_obj_t *cl = make_label(chipb, chip, &lv_font_unscii_8, chip_fg);
  lv_obj_center(cl);

  lv_obj_t *col = plain(row);
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_height(col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 1, 0);
  lv_obj_t *t = make_label(col, title, &lv_font_montserrat_16, C_TXT);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT); lv_obj_set_width(t, lv_pct(100));
  lv_obj_t *s = make_label(col, sub, &lv_font_montserrat_14, C_SUB);
  lv_label_set_long_mode(s, LV_LABEL_LONG_DOT); lv_obj_set_width(s, lv_pct(100));

  if (badge) {
    lv_obj_t *b = make_label(row, badge, &lv_font_unscii_8, badge_fg);
    lv_obj_set_style_bg_color(b, lv_color_hex(badge_bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_pad_hor(b, 6, 0);
    lv_obj_set_style_pad_ver(b, 3, 0);
  }

  lv_obj_add_event_cb(row, item_clicked_cb, LV_EVENT_CLICKED, (void *)dest);
  lv_obj_add_event_cb(row, focus_scroll_cb, LV_EVENT_FOCUSED, NULL);
  lv_group_add_obj(g_group, row);
}

// ---------------------------------------------------------------- screens
static void build_home();
static void build_around();
static void build_category(int c);
static void build_tool(int c, int i);
static void build_settings();
static void build_edit_bright();

static lv_obj_t *new_screen(const char *title) {
  lv_group_remove_all_objs(g_group);          // detach old (about to be deleted)
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_add_style(scr, &st_screen, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  build_statusbar(scr, title);
  return scr;
}
static void load_screen(lv_obj_t *scr) {
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);  // auto-delete old
}

static void section(lv_obj_t *scr, const char *txt) {
  lv_obj_t *l = make_label(scr, txt, &lv_font_unscii_8, C_MUTE);
  lv_obj_set_style_pad_left(l, 12, 0);
  lv_obj_set_style_pad_top(l, 6, 0);
}

static void build_home() {
  lv_obj_t *scr = new_screen("HANDHELD");
  section(scr, "AROUND ME");
  lv_obj_t *list = make_list(scr);
  add_row(list, "\xE2\x97\x89", C_GREEN, "Around me", "live radar of what's near you",
          "live", C_GREEN, C_CHIP, nav_code(SCR_AROUND, 0, 0));
  for (uint8_t i = 0; i < N_CATS; i++)
    add_row(list, CATS[i].icon, C_GREEN, CATS[i].name, CATS[i].tag,
            NULL, 0, 0, nav_code(SCR_CATEGORY, i, 0));
  add_row(list, "SET", C_SUB, "Settings", "brightness, sleep, about",
          NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  load_screen(scr);
}

static void build_category(int c) {
  const Category &cat = CATS[c];
  lv_obj_t *scr = new_screen(cat.name);
  section(scr, cat.tag);
  lv_obj_t *list = make_list(scr);
  for (uint8_t i = 0; i < cat.n; i++) {
    const Tool &t = cat.tools[i];
    uint32_t fg, bg; kind_colors(t.kind, fg, bg);
    add_row(list, t.code, fg, t.name, t.sub, kind_text(t.kind), fg, bg,
            nav_code(SCR_TOOL, c, i));
  }
  load_screen(scr);
}

// interpretation card used by the "Around me" showcase
static void around_card(lv_obj_t *list, const char *chip, uint32_t chip_fg,
                        const char *title, const char *sub,
                        const char *badge, uint32_t bfg, uint32_t bbg) {
  add_row(list, chip, chip_fg, title, sub, badge, bfg, bbg, nav_code(SCR_HOME, 0, 0));
}
static void build_around() {
  lv_obj_t *scr = new_screen("AROUND ME");
  section(scr, "5 THINGS - THE REAL FREQ IS ON EACH CARD");
  lv_obj_t *list = make_list(scr);
  around_card(list, "RF",  C_RED,   "Car key fob",   "433.92 MHz  rolling code", "no copy", C_RED,  C_RED_BG);
  around_card(list, "RF",  C_GREEN, "Gate remote",   "433.92 MHz  fixed code",   "copy",    C_GREEN,C_CHIP);
  around_card(list, "IR",  C_AMBER, "Samsung TV",    "infrared  ready",          "control", C_GREEN,C_CHIP);
  around_card(list, "WiFi",C_GREEN, "5 nets 9 devices","tap to see who is here", "explore", C_GREEN,C_CHIP);
  around_card(list, "BLE", C_CYAN,  "AirTag nearby", "seen 3x  moving with you", "track?",  C_CYAN, C_CYAN_BG);
  load_screen(scr);
}

// a padded, flex-column content area below the status bar (no margins — this
// LVGL build has margin style props disabled, so we inset with padding only)
static lv_obj_t *content_box(lv_obj_t *scr) {
  lv_obj_t *b = plain(scr);
  lv_obj_set_width(b, lv_pct(100));
  lv_obj_set_flex_grow(b, 1);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_hor(b, 9, 0);
  lv_obj_set_style_pad_top(b, 9, 0);
  lv_obj_set_style_pad_row(b, 6, 0);
  return b;
}

// framed panel inside a content box for the showcase tool screens
static lv_obj_t *panel(lv_obj_t *box) {
  lv_obj_t *p = lv_obj_create(box);
  lv_obj_remove_style_all(p);
  lv_obj_add_style(p, &st_item, 0);
  lv_obj_set_width(p, lv_pct(100));
  lv_obj_set_style_pad_all(p, 12, 0);
  lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(p, 6, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  return p;
}

static void tool_freq_finder(lv_obj_t *box) {   // Sub-GHz > Frequency finder
  // Preset ISM spots covering the common sub-GHz remote/sensor bands.
  static const float FREQS[] = {300.0f, 315.0f, 390.0f, 433.92f, 868.0f, 915.0f};
  const int N = sizeof(FREQS) / sizeof(FREQS[0]);

  bool live = cc1101_present();
  float peak_mhz = 433.92f;   // demo fallback when no radio is attached
  int   peak_rssi = -42;
  if (live) {
    int rssi[N];
    int bi = cc1101_sweep(FREQS, N, rssi);
    if (bi >= 0) { peak_mhz = FREQS[bi]; peak_rssi = rssi[bi]; }
  }

  lv_obj_t *p = panel(box);
  char mhz[16]; snprintf(mhz, sizeof mhz, "%.2f", peak_mhz);
  make_label(p, mhz, &lv_font_montserrat_28, C_GREEN);
  char sub[40]; snprintf(sub, sizeof sub, "MHz  -  %d dBm", peak_rssi);
  make_label(p, sub, &lv_font_unscii_8, C_SUB);

  lv_obj_t *bar = lv_bar_create(p);
  lv_obj_set_size(bar, lv_pct(100), 10);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_GREEN), LV_PART_INDICATOR);
  lv_bar_set_value(bar, sg_bar_pct(peak_rssi), LV_ANIM_OFF);

  make_label(p, sg_guess(peak_mhz), &lv_font_montserrat_16, C_TXT);
  make_label(p, live ? "strongest signal in range" : "demo - CC1101 not detected",
             &lv_font_montserrat_14, live ? C_GREEN_SFT : C_AMBER);
  make_label(box, "rotate = sweep    click = lock", &lv_font_unscii_8, C_MUTE);
}

static void tool_subghz_capture(lv_obj_t *box) { // Sub-GHz > Capture & replay
  lv_obj_t *p = panel(box);
  bool live = cc1101_present();
  uint32_t code = 0x0015F3; uint8_t bits = 24; int proto = 1;   // demo fallback
  // ponytail: blocks up to 1.2 s while listening. Fine for bring-up; make it
  // event-driven if the capture screen ever needs to stay responsive.
  bool got = live && subghz_capture(433.92f, 1200, &code, &bits, &proto);
  if (live && !got) {
    make_label(p, "LISTENING 433.92", &lv_font_unscii_8, C_GREEN);
    make_label(p, "press a fob near the antenna", &lv_font_montserrat_16, C_TXT);
    make_label(box, "rotate = band    click = replay", &lv_font_unscii_8, C_MUTE);
    return;
  }
  char h[40]; rcs_fmt(code, bits, proto, h, sizeof h);
  make_label(p, got ? "CAPTURED" : "DEMO CAPTURE", &lv_font_unscii_8,
             got ? C_GREEN : C_AMBER);
  make_label(p, h, &lv_font_montserrat_20, C_TXT);
  make_label(p, "433.92 MHz  -  fixed code (OOK)", &lv_font_montserrat_14, C_SUB);
  make_label(p, got ? "click to replay this remote" : "demo - CC1101 not detected",
             &lv_font_montserrat_14, got ? C_GREEN_SFT : C_AMBER);
  make_label(box, "captures & replays -> SD", &lv_font_unscii_8, C_MUTE);
}

static void tool_wmbus(lv_obj_t *box) {          // Sub-GHz > wM-Bus meter
  lv_obj_t *p = panel(box);
  // Demo header: manuf ELS (LE 0x93,0x15), 4-byte serial (LE), version, medium.
  const uint8_t hdr[] = {0x93, 0x15, 0x78, 0x56, 0x34, 0x12, 0x01, 0x07};
  uint16_t manid = (uint16_t)(hdr[0] | (hdr[1] << 8));
  char mf[4]; wmbus_manuf(manid, mf);

  make_label(p, "wM-BUS 868.95", &lv_font_unscii_8, C_AMBER);
  char l1[40]; snprintf(l1, sizeof l1, "%s  -  %s", mf, wmbus_medium(hdr[7]));
  make_label(p, l1, &lv_font_montserrat_20, C_TXT);
  char sn[40]; snprintf(sn, sizeof sn, "serial %02X%02X%02X%02X",
                        hdr[5], hdr[4], hdr[3], hdr[2]);
  make_label(p, sn, &lv_font_montserrat_14, C_SUB);
  make_label(p, "demo - CC1101 wM-Bus RX = bring-up", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "reads utility meters (T/C mode, 868 MHz)", &lv_font_unscii_8, C_MUTE);
}

static void tool_mifare(lv_obj_t *box) {         // RFID/NFC > Mifare crack
  lv_obj_t *p = panel(box);
  const uint8_t found[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};   // demo: sector 0 opened
  make_label(p, "MIFARE CRACK", &lv_font_unscii_8, C_AMBER);
  char h[40]; snprintf(h, sizeof h, "%d keys in dictionary", MIFARE_KEY_COUNT);
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  char k[48]; snprintf(k, sizeof k, "sec 0 KeyA: %s", mifare_key_name(found));
  make_label(p, k, &lv_font_unscii_8, C_GREEN_SFT);
  make_label(p, "tries default keys per sector", &lv_font_montserrat_14, C_SUB);
  make_label(box, "PN532 authenticate loop = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_amiibo(lv_obj_t *box) {         // RFID/NFC > Amiibo clone
  lv_obj_t *p = panel(box);
  uint8_t d[NTAG215_SIZE] = {0};                 // demo dump: NTAG215 CC + Mario id
  d[12] = 0xE1; d[13] = 0x10; d[14] = 0x3E; d[15] = 0x00;
  d[AMIIBO_ID_OFF + 7] = 0x02;
  char id[17]; amiibo_id_hex(d, id);
  bool ok = ntag215_is_amiibo(d, sizeof d);
  make_label(p, ok ? "NTAG215 AMIIBO" : "NOT AMIIBO", &lv_font_unscii_8, C_GREEN);
  make_label(p, id, &lv_font_montserrat_20, C_TXT);
  make_label(p, "figure id (page 21)", &lv_font_montserrat_14, C_SUB);
  make_label(p, "demo - PN532 dump/write = bring-up", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "clone amiibo -> blank NTAG215", &lv_font_unscii_8, C_MUTE);
}

static void tool_emv(lv_obj_t *box) {            // RFID/NFC > Bank card read
  lv_obj_t *p = panel(box);
  // Demo Track-2-Equivalent (test PAN 4111..., not a real card).
  const uint8_t t2[12] = {0x41,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
                          0xD2,0x51,0x22,0x01};
  char pan[24], yymm[5];
  bool ok = emv_parse_track2(t2, sizeof t2, pan, sizeof pan, yymm);
  char masked[24] = "----";
  size_t plen = 0; while (pan[plen]) plen++;
  if (ok && plen >= 10) {                        // PCI-safe: first 6 + last 4
    size_t k = 0;
    for (size_t i = 0; i < plen; i++)
      masked[k++] = (i < 6 || i >= plen - 4) ? pan[i] : '*';
    masked[k] = 0;
  }
  make_label(p, "EMV CONTACTLESS", &lv_font_unscii_8, C_AMBER);
  make_label(p, masked, &lv_font_montserrat_20, C_TXT);
  char sub[48]; snprintf(sub, sizeof sub, "%s  -  exp %c%c/%c%c",
                         card_network(pan), yymm[2], yymm[3], yymm[0], yymm[1]);
  make_label(p, sub, &lv_font_montserrat_14, C_SUB);
  make_label(p, luhn_valid(pan) ? "Luhn OK  -  demo card" : "invalid",
             &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "public data only - PN532 APDU = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_nfc_read(lv_obj_t *box) {       // RFID/NFC > Read / clone
  lv_obj_t *p = panel(box);
  if (!nfc_present()) {
    make_label(p, "NFC READ", &lv_font_unscii_8, C_AMBER);
    make_label(p, "demo - PN532 not detected", &lv_font_montserrat_16, C_AMBER);
    make_label(p, "UID 04:A2:1B:9C  -  Mifare Classic 1K", &lv_font_montserrat_14, C_SUB);
    make_label(box, "click to crack keys (dictionary)", &lv_font_unscii_8, C_MUTE);
    return;
  }
  uint8_t uid[7], len = 0;
  if (nfc_read_uid(uid, &len)) {
    char h[24]; nfc_uid_hex(uid, len, h, sizeof h);
    make_label(p, "CARD", &lv_font_unscii_8, C_GREEN);
    make_label(p, h, &lv_font_montserrat_20, C_TXT);
    make_label(p, len == 4 ? "Mifare Classic / NTAG" : "7-byte UID card",
               &lv_font_montserrat_14, C_SUB);
    make_label(p, "click to crack keys (dictionary)", &lv_font_montserrat_14, C_GREEN_SFT);
  } else {
    make_label(p, "PN532 READY", &lv_font_unscii_8, C_GREEN);
    make_label(p, "tap a card to the antenna", &lv_font_montserrat_16, C_TXT);
  }
}

static void tool_wifi_scan(lv_obj_t *box) {      // WiFi > Scan / recon
  lv_obj_t *p = panel(box);
  int n = wifi_count();
  if (n <= 0) n = wifi_scan();                   // first entry: one ~2 s scan, then cached
  if (n <= 0) {
    make_label(p, "WIFI SCAN", &lv_font_unscii_8, C_GREEN);
    make_label(p, "no networks found", &lv_font_montserrat_16, C_SUB);
    return;
  }
  char h[24]; snprintf(h, sizeof h, "%d networks", n);
  make_label(p, h, &lv_font_unscii_8, C_GREEN);
  int show = n < 5 ? n : 5;
  for (int i = 0; i < show; i++) {
    int enc = wifi_enc(i);
    char line[72];
    snprintf(line, sizeof line, "%s  %s  %d dBm", wifi_ssid(i), wifi_enc_str(enc), wifi_rssi(i));
    make_label(p, line, &lv_font_montserrat_14, wifi_is_open(enc) ? C_RED : C_TXT);
  }
  make_label(box, "click = rescan    red = open network", &lv_font_unscii_8, C_MUTE);
}

static void tool_csi(lv_obj_t *box) {            // See invisible > See through wall
  lv_obj_t *p = panel(box);
  // Demo CSI window with someone moving -> high variance -> motion.
  float win[8] = {40.f, 60.f, 42.f, 58.f, 39.f, 61.f, 41.f, 59.f};
  float var = csi_variance(win, 8);
  bool motion = csi_motion(win, 8, 5.0f);
  make_label(p, "SEE THROUGH WALL", &lv_font_unscii_8, C_CYAN);
  make_label(p, motion ? "MOTION DETECTED" : "room is still",
             &lv_font_montserrat_20, motion ? C_RED : C_GREEN_SFT);
  char h[40]; snprintf(h, sizeof h, "CSI variance %.0f", var);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "ambient WiFi channel-state sensing", &lv_font_montserrat_14, C_SUB);
  make_label(box, "esp_wifi CSI capture = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_df(lv_obj_t *box) {             // See invisible > Direction finder
  static int prev = -80;
  lv_obj_t *p = panel(box);
  make_label(p, "DIRECTION FINDER", &lv_font_unscii_8, C_GREEN);
  if (!cc1101_present()) {
    make_label(p, "demo - CC1101 not detected", &lv_font_montserrat_16, C_AMBER);
    make_label(p, "433.92 MHz  -70 dBm  WARMER", &lv_font_montserrat_14, C_SUB);
    return;
  }
  int rssi = cc1101_rssi_at(433.92f);
  int t = df_trend(rssi, prev);
  prev = rssi;
  char h[40]; snprintf(h, sizeof h, "%d dBm  %s", rssi, df_label(t));
  make_label(p, h, &lv_font_montserrat_20, t > 0 ? C_GREEN : t < 0 ? C_RED : C_TXT);
  make_label(p, "walk around - click to sample", &lv_font_montserrat_14, C_SUB);
  make_label(box, "warmer = closer to the transmitter", &lv_font_unscii_8, C_MUTE);
}

static void tool_nrf_scan(lv_obj_t *box) {       // NRF24 / 2.4GHz > Band scanner
  lv_obj_t *p = panel(box);
  if (!nrf_present()) {
    make_label(p, "2.4GHz SCAN", &lv_font_unscii_8, C_AMBER);
    make_label(p, "demo - NRF24 not detected", &lv_font_montserrat_16, C_AMBER);
    make_label(p, "busiest: ch 6 (2406 MHz)", &lv_font_montserrat_14, C_SUB);
    return;
  }
  if (!nrf_scanned()) nrf_scan();
  int bc = nrf_busiest_ch();
  make_label(p, "2.4GHz SCAN", &lv_font_unscii_8, C_GREEN);
  char h[40]; snprintf(h, sizeof h, "busiest: ch %d (%d MHz)", bc, nrf_ch_mhz(bc));
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  make_label(p, "WiFi / BT / wireless keyboards live here", &lv_font_montserrat_14, C_SUB);
  make_label(box, "click = rescan", &lv_font_unscii_8, C_MUTE);
}

static void tool_keysniff(lv_obj_t *box) {       // NRF24 / 2.4GHz > Keyboard sniff
  lv_obj_t *p = panel(box);
  // Demo: decode a sniffed "Hello" HID report stream to text.
  struct { uint8_t mod, key; } rpt[] = {
    {0x02,0x0B},{0x00,0x08},{0x00,0x0F},{0x00,0x0F},{0x00,0x12}};
  char txt[8]; int n = 0;
  for (auto &r : rpt) { char c = hid_to_ascii(r.key, hid_shift(r.mod)); if (c) txt[n++] = c; }
  txt[n] = 0;
  make_label(p, "KEYBOARD SNIFF", &lv_font_unscii_8, C_AMBER);
  make_label(p, "log 2.4GHz keystrokes", &lv_font_montserrat_16, C_TXT);
  char h[32]; snprintf(h, sizeof h, "captured: %s", txt);
  make_label(p, h, &lv_font_montserrat_20, C_GREEN_SFT);
  make_label(p, "unencrypted keyboards only", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "nRF24 ESB sniff = bring-up - own gear only",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_mousejack(lv_obj_t *box) {      // NRF24 / 2.4GHz > Mousejack
  lv_obj_t *p = panel(box);
  uint8_t f[UNIFYING_KBD_LEN];
  mousejack_key(0x00, 0x00, 0x04, f);            // demo: an 'a' keystroke frame
  make_label(p, "MOUSEJACK", &lv_font_unscii_8, C_AMBER);
  make_label(p, "inject into wireless kbd/mouse", &lv_font_montserrat_16, C_TXT);
  char hx[40]; snprintf(hx, sizeof hx, "Unifying frame  cksum %02X  (sum 0)", f[9]);
  make_label(p, hx, &lv_font_unscii_8, C_SUB);
  make_label(p, "unencrypted Logitech dongles", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "nRF24 ESB inject = bring-up - own gear only",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_espnow(lv_obj_t *box) {         // Comms / Off-grid > ESP-NOW mesh
  if (!espnow_active()) espnow_begin();
  lv_obj_t *p = panel(box);
  make_label(p, "ESP-NOW MESH", &lv_font_unscii_8, C_GREEN);
  char h[40]; snprintf(h, sizeof h, "%lu messages received", (unsigned long)espnow_rx());
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  const char *last = espnow_last();
  make_label(p, last[0] ? last : "(no messages yet)", &lv_font_montserrat_14, C_SUB);
  make_label(box, "router-free - click to broadcast ping", &lv_font_unscii_8, C_MUTE);
}

static void tool_usage(lv_obj_t *box) {          // Me > Claude usage
  lv_obj_t *p = panel(box);
  int pct = usage_pct(62, 100);                  // demo — real data via phone bridge
  make_label(p, "CLAUDE USAGE", &lv_font_unscii_8, C_GREEN);
  char h[16]; snprintf(h, sizeof h, "%d%% of 5h", pct);
  make_label(p, h, &lv_font_montserrat_28, pct > 85 ? C_RED : C_GREEN);
  lv_obj_t *bar = lv_bar_create(p);
  lv_obj_set_size(bar, lv_pct(100), 10);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_GREEN), LV_PART_INDICATOR);
  lv_bar_set_value(bar, pct, LV_ANIM_OFF);
  char r[24]; fmt_hms(12180, r, sizeof r);
  char line[40]; snprintf(line, sizeof line, "resets in %s", r);
  make_label(p, line, &lv_font_montserrat_14, C_SUB);
  make_label(box, "demo - real data via phone bridge", &lv_font_unscii_8, C_MUTE);
}

static void tool_tasks(lv_obj_t *box) {          // Me > Tasks
  lv_obj_t *p = panel(box);
  make_label(p, "TASKS", &lv_font_unscii_8, C_GREEN);
  static const char *lines[] = {
    "[ ] !1 Solder the BL mod", "[x] Flash firmware m2", "[ ] !3 Order antennas"};
  for (const char *ln : lines) {
    bool done; int prio; const char *text;
    if (!task_parse(ln, &done, &prio, &text)) continue;
    char row[48];
    snprintf(row, sizeof row, "%s %s%s", done ? "[x]" : "[ ]",
             prio ? (prio == 1 ? "! " : "  ") : "  ", text);
    make_label(p, row, &lv_font_unscii_8, done ? C_MUTE : C_TXT);
  }
  make_label(box, "synced from your phone = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_calendar(lv_obj_t *box) {       // Me > Calendar
  lv_obj_t *p = panel(box);
  char when[32];
  ical_friendly("20260719T143000Z", when, sizeof when);   // demo next event
  make_label(p, "CALENDAR", &lv_font_unscii_8, C_GREEN);
  make_label(p, "Team sync", &lv_font_montserrat_20, C_TXT);
  make_label(p, when, &lv_font_montserrat_16, C_GREEN_SFT);
  make_label(p, "next event from your phone", &lv_font_montserrat_14, C_SUB);
  make_label(box, "BLE bridge to phone = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_badusb(lv_obj_t *box) {         // BadUSB / HID > DuckyScript
  // Info only — never auto-runs a payload on screen build (that would type into
  // whatever's plugged in). Running is a deliberate ACTION-key step (next iter).
  lv_obj_t *p = panel(box);
  make_label(p, "BADUSB / HID", &lv_font_unscii_8, C_RED);
  make_label(p, "acts as a USB keyboard", &lv_font_montserrat_16, C_TXT);
  make_label(p, "payload: STRING / GUI r / DELAY / ENTER", &lv_font_montserrat_14, C_SUB);
  make_label(p, "load a .txt from SD, then run", &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "only on machines you own", &lv_font_unscii_8, C_MUTE);
}

static void tool_tvbgone(lv_obj_t *box) {        // Pranks / IR > TV-B-Gone
  // Info only — never blasts on screen build; firing is a deliberate ACTION step.
  lv_obj_t *p = panel(box);
  make_label(p, "TV-B-GONE", &lv_font_unscii_8, C_GREEN);
  char h[40]; snprintf(h, sizeof h, "%d TV power codes ready", tvb_count());
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  make_label(p, "Samsung / LG / Sony / NEC / Philips", &lv_font_montserrat_14, C_SUB);
  make_label(box, "click = blast all (turns TVs off)", &lv_font_unscii_8, C_MUTE);
}

static void tool_ir_universal(lv_obj_t *box) {   // IR > Universal remote
  lv_obj_t *p = panel(box);
  const IrBrand *b = ir_brand_at(0);             // demo: first brand (Samsung)
  make_label(p, "UNIVERSAL REMOTE", &lv_font_unscii_8, C_GREEN);
  make_label(p, b->name, &lv_font_montserrat_20, C_TXT);
  char h[40]; snprintf(h, sizeof h, "POWER  0x%08lX", (unsigned long)b->power);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  char n[40]; snprintf(n, sizeof n, "%d brands in DB", ir_brand_count());
  make_label(p, n, &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "rotate = brand   click = blast (bring-up)",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_ir_learn(lv_obj_t *box) {       // IR > Learn & blast
  lv_obj_t *p = panel(box);
  make_label(p, "IR LEARN / BLAST", &lv_font_unscii_8, C_GREEN);
  make_label(p, "TX GPIO47   RX GPIO48", &lv_font_unscii_8, C_SUB);
  make_label(p, "aim any remote and press a button", &lv_font_montserrat_16, C_TXT);
  make_label(p, "last: NEC  addr 0x04  cmd 0x08  (demo)", &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "click = blast it back", &lv_font_unscii_8, C_MUTE);
}

static void tool_ble_scan(lv_obj_t *box) {       // Bluetooth > Scan / recon
  lv_obj_t *p = panel(box);
  int n = ble_count();
  if (n <= 0) n = ble_scan(3);
  if (n <= 0) {
    make_label(p, "BLE SCAN", &lv_font_unscii_8, C_GREEN);
    make_label(p, "nothing advertising nearby", &lv_font_montserrat_16, C_SUB);
    return;
  }
  char h[24]; snprintf(h, sizeof h, "%d devices", n);
  make_label(p, h, &lv_font_unscii_8, C_GREEN);
  int show = n < 5 ? n : 5;
  for (int i = 0; i < show; i++) {
    const char *nm = ble_name(i);
    if (!nm[0]) nm = ble_addr(i);
    char line[72];
    snprintf(line, sizeof line, "%s  %d dBm%s", nm, ble_rssi(i),
             ble_is_tracker(i) ? "  [TRACKER]" : "");
    make_label(p, line, &lv_font_montserrat_14, ble_is_tracker(i) ? C_CYAN : C_TXT);
  }
  make_label(box, "click = rescan    cyan = tracker", &lv_font_unscii_8, C_MUTE);
}

static void tool_camera(lv_obj_t *box) {         // Am I safe? > Hidden camera
  lv_obj_t *p = panel(box);
  make_label(p, "HIDDEN CAMERA", &lv_font_unscii_8, C_CYAN);
  int n = wifi_count();
  if (n <= 0) n = wifi_scan();
  int cams = 0;
  for (int i = 0; i < n; i++) {
    const char *b = camera_ssid_brand(wifi_ssid(i));
    if (!b) continue;
    cams++;
    char line[64]; snprintf(line, sizeof line, "%s  (%s)", wifi_ssid(i), b);
    make_label(p, line, &lv_font_montserrat_14, C_AMBER);
  }
  if (cams == 0)
    make_label(p, n > 0 ? "no camera-like WiFi APs" : "scanning...",
               &lv_font_montserrat_16, C_GREEN_SFT);
  make_label(box, "WiFi-name heuristic - OUI check next", &lv_font_unscii_8, C_MUTE);
}

static void tool_deauth(lv_obj_t *box) {         // Am I safe? > Deauth detector
  if (!deauth_active()) deauth_begin();
  lv_obj_t *p = panel(box);
  make_label(p, "DEAUTH DETECTOR", &lv_font_unscii_8, C_CYAN);
  uint32_t hits = deauth_count();
  char h[40]; snprintf(h, sizeof h, "%lu deauth frames seen", (unsigned long)hits);
  make_label(p, h, &lv_font_montserrat_16, hits > 0 ? C_RED : C_GREEN_SFT);
  make_label(p, hits > 0 ? "someone may be jamming WiFi near you" : "airwaves look clean",
             &lv_font_montserrat_14, C_SUB);
  make_label(box, "watching 802.11 management frames", &lv_font_unscii_8, C_MUTE);
}

static void tool_hackscreen(lv_obj_t *box) {     // Pranks > Hacker screen
  lv_obj_t *p = panel(box);
  make_label(p, "ACCESS GRANTED", &lv_font_unscii_8, C_GREEN);
  uint32_t s = 0xC0FFEE;                          // static frames; animate = enhancement
  for (int i = 0; i < 3; i++) {
    char line[25]; hack_line(&s, line, 24);
    make_label(p, line, &lv_font_unscii_8, C_GREEN_SFT);
  }
  make_label(box, "fake movie hack - just for show", &lv_font_unscii_8, C_MUTE);
}

static void tool_castcrash(lv_obj_t *box) {      // Pranks > Cast crasher
  lv_obj_t *p = panel(box);
  const char *resp =
    "HTTP/1.1 200 OK\r\n"
    "LOCATION: http://192.168.1.42:8008/ssdp/device-desc.xml\r\n"
    "ST: urn:dial-multiscreen-org:service:dial:1\r\n\r\n";
  char loc[96], st[96];
  ssdp_header(resp, "LOCATION", loc, sizeof loc);
  ssdp_header(resp, "ST", st, sizeof st);
  make_label(p, "CAST CRASHER", &lv_font_unscii_8, C_GREEN);
  make_label(p, cast_kind(st), &lv_font_montserrat_20, C_TXT);
  make_label(p, "192.168.1.42:8008", &lv_font_unscii_8, C_SUB);
  make_label(p, "queue a video on nearby TVs (fun)",
             &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "SSDP discovery + DIAL = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_audiobug(lv_obj_t *box) {       // Am I safe? > Audio bug sweep
  lv_obj_t *p = panel(box);
  float peak = 96.5f;                            // demo peak in the FM-mic band
  const char *band = bug_band(peak);
  make_label(p, "BUG SWEEP", &lv_font_unscii_8, C_CYAN);
  char h[40]; snprintf(h, sizeof h, "peak %.1f MHz", peak);
  make_label(p, h, &lv_font_montserrat_20, C_TXT);
  make_label(p, band ? band : "no covert bands active",
             &lv_font_montserrat_16, band ? C_RED : C_GREEN_SFT);
  make_label(p, "FM / VHF / UHF / GSM / 2.4G", &lv_font_unscii_8, C_SUB);
  make_label(box, "wideband RF sweep = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_skimmer(lv_obj_t *box) {        // Am I safe? > Skimmer detector
  lv_obj_t *p = panel(box);
  const char *seen = "HC-05";                    // demo: a flagged nearby module
  bool hit = is_skimmer_name(seen);
  make_label(p, "SKIMMER DETECTOR", &lv_font_unscii_8, C_CYAN);
  make_label(p, hit ? "SUSPECT MODULE NEARBY" : "no skimmer signatures",
             &lv_font_montserrat_16, hit ? C_RED : C_GREEN_SFT);
  char h[40]; snprintf(h, sizeof h, "BLE name: \"%s\"", seen);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "generic BT modules used by skimmers",
             &lv_font_montserrat_14, C_SUB);
  make_label(box, "scan at pumps/ATMs - BLE scan bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_droneid(lv_obj_t *box) {        // Am I safe? > Drone spotter
  lv_obj_t *p = panel(box);
  // Demo Remote-ID: Basic ID + a Location fix.
  uint8_t basic[22] = {0x02, 0x10};
  const char *uas = "1596F3A2C0D9K7X4";
  for (int i = 0; uas[i]; i++) basic[2 + i] = (uint8_t)uas[i];
  char id[21]; odid_basic_id(basic, id);
  int32_t lat = 129716000, lon = 775946000;      // 12.9716, 77.5946
  make_label(p, "DRONE SPOTTER", &lv_font_unscii_8, C_CYAN);
  make_label(p, "Remote-ID broadcast nearby", &lv_font_montserrat_16, C_TXT);
  char h[48]; snprintf(h, sizeof h, "ID %s", id);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  char loc[48]; snprintf(loc, sizeof loc, "pilot @ %.4f, %.4f",
                         odid_coord(lat), odid_coord(lon));
  make_label(p, loc, &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "BLE/WiFi Remote-ID sniff = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_fwdump(lv_obj_t *box) {         // Tools > Firmware dump
  lv_obj_t *p = panel(box);
  const uint8_t jedec[3] = {0xEF, 0x40, 0x18};   // demo: W25Q128 (Winbond 16 MiB)
  uint32_t bytes = jedec_capacity_bytes(jedec[2]);
  make_label(p, "FIRMWARE DUMP", &lv_font_unscii_8, C_GREEN);
  char id[40]; snprintf(id, sizeof id, "JEDEC %02X %02X %02X", jedec[0], jedec[1], jedec[2]);
  make_label(p, id, &lv_font_unscii_8, C_SUB);
  char h[48]; snprintf(h, sizeof h, "%s  -  %lu MB", jedec_manuf(jedec[0]),
                       (unsigned long)(bytes / (1024 * 1024)));
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  make_label(p, "read chip -> dump.bin on SD", &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "clip onto SPI flash - read = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_gpio(lv_obj_t *box) {           // Tools > GPIO play
  lv_obj_t *p = panel(box);
  int pin = 5;                                   // demo pin (usable)
  make_label(p, "GPIO PLAY", &lv_font_unscii_8, C_GREEN);
  char h[40]; snprintf(h, sizeof h, "GPIO %d  -  %s", pin,
                       gpio_usable(pin) ? "safe to drive" : "RESERVED");
  make_label(p, h, &lv_font_montserrat_20, gpio_usable(pin) ? C_TXT : C_RED);
  make_label(p, "toggle HIGH/LOW, read state", &lv_font_montserrat_14, C_SUB);
  make_label(p, "flash/PSRAM pins 26-37 locked out", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "rotate = pin   click = toggle (bring-up)",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_buspirate(lv_obj_t *box) {      // Tools > Bus Pirate
  lv_obj_t *p = panel(box);
  make_label(p, "I2C SCAN", &lv_font_unscii_8, C_GREEN);
  make_label(p, "sniff & probe I2C/SPI/UART", &lv_font_montserrat_16, C_TXT);
  // Demo: name the addresses this board is expected to answer at.
  const uint8_t addrs[] = {0x20, 0x24, 0x36, 0x68};
  char h[64]; int o = 0;
  for (unsigned k = 0; k < sizeof addrs; k++)
    o += snprintf(h + o, sizeof h - o, k ? " %02X" : "%02X", addrs[k]);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, i2c_device_name(0x24), &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "live bus scan (Wire) = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_wardrive(lv_obj_t *box) {       // WiFi > Wardrive
  lv_obj_t *p = panel(box);
  char row[128];
  wardrive_csv("A4:2B:B0:11:22:33", "linksys", "[WPA2-PSK-CCMP][ESS]",
               6, -52, 12.971600, 77.594600, row, sizeof row);
  make_label(p, "WARDRIVE", &lv_font_unscii_8, C_GREEN);
  make_label(p, "log every AP + GPS -> SD (WiGLE)", &lv_font_montserrat_16, C_TXT);
  make_label(p, "1 net  -  fix 12.9716,77.5946", &lv_font_unscii_8, C_SUB);
  make_label(p, "wardrive.csv ready to upload", &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(box, "needs GPS fix + SD - scan is bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_handshake(lv_obj_t *box) {      // WiFi > Handshake / PMKID
  lv_obj_t *p = panel(box);
  // Demo: M1 + M2 captured (enough to crack offline).
  uint8_t got = (1 << 0) | (1 << 1);
  bool crack = handshake_crackable(got);
  make_label(p, "HANDSHAKE", &lv_font_unscii_8, C_AMBER);
  make_label(p, "capture WPA2 4-way -> SD", &lv_font_montserrat_16, C_TXT);
  char h[48]; snprintf(h, sizeof h, "M1:%c M2:%c M3:- M4:-",
                       got & 1 ? 'y' : '-', got & 2 ? 'y' : '-');
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, crack ? "crackable - saved .pcap" : "waiting for handshake",
             &lv_font_montserrat_14, crack ? C_GREEN_SFT : C_AMBER);
  make_label(box, "deauth to force reconnect - own AP only",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_evilportal(lv_obj_t *box) {     // WiFi > Evil Portal
  lv_obj_t *p = panel(box);
  // Demo: a credential POST the fake login page would capture.
  const char *post = "user=alice&pass=hunter%402";
  char u[32], pw[32];
  form_get(post, "user", u, sizeof u);
  form_get(post, "pass", pw, sizeof pw);
  make_label(p, "EVIL PORTAL", &lv_font_unscii_8, C_AMBER);
  make_label(p, "fake captive login, capture creds", &lv_font_montserrat_16, C_TXT);
  char h[48]; snprintf(h, sizeof h, "caught: %s / %s", u, pw);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "awareness testing - your network only",
             &lv_font_montserrat_14, C_AMBER);
  make_label(box, "DNS hijack + HTTP server = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_karma(lv_obj_t *box) {          // WiFi > Karma / MANA
  lv_obj_t *p = panel(box);
  // Demo probe request: client hunting for "CoffeeShop".
  const uint8_t probe[] = {0x00,0x0A,'C','o','f','f','e','e','S','h','o','p',
                           0x01,0x04,0x82,0x84,0x8b,0x96};
  char ssid[33];
  bool got = ie_get_ssid(probe, sizeof probe, ssid, sizeof ssid);
  make_label(p, "KARMA / MANA", &lv_font_unscii_8, C_AMBER);
  make_label(p, "answer any SSID a phone probes for", &lv_font_montserrat_16, C_TXT);
  char h[48]; snprintf(h, sizeof h, "heard probe: \"%s\"", got ? ssid : "(hidden)");
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "lures auto-connect - your own devices only",
             &lv_font_montserrat_14, C_AMBER);
  make_label(box, "SoftAP auto-respond = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_deauth_atk(lv_obj_t *box) {     // WiFi > Deauth (authorized)
  lv_obj_t *p = panel(box);
  const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  const uint8_t bssid[6] = {0x00,0x11,0x22,0x33,0x44,0x55};
  uint8_t f[DEAUTH_FRAME_LEN];
  deauth_frame(bcast, bssid, 7, f);              // demo frame the tool would send
  make_label(p, "DEAUTH", &lv_font_unscii_8, C_AMBER);
  make_label(p, "kick a client off an AP", &lv_font_montserrat_16, C_TXT);
  char hx[40]; snprintf(hx, sizeof hx, "FC %02X%02X   reason %d", f[0], f[1], f[24]);
  make_label(p, hx, &lv_font_unscii_8, C_SUB);
  make_label(p, "YOUR OWN NETWORK ONLY", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "pick AP + client, click to send - TX bring-up",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_ibutton(lv_obj_t *box) {        // RFID/NFC > iButton key
  lv_obj_t *p = panel(box);
  make_label(p, "IBUTTON / 1-WIRE", &lv_font_unscii_8, C_GREEN);
  uint8_t rom[8] = {0x01, 0x2A, 0x3B, 0x4C, 0x5D, 0x6E, 0x7F, 0x00};
  rom[7] = onewire_crc8(rom, 7);
  char h[48]; snprintf(h, sizeof h, "family 0x%02X  crc %s", rom[0],
                       ibutton_valid(rom) ? "ok" : "bad");
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  make_label(p, "touch a Dallas key to the probe", &lv_font_montserrat_14, C_SUB);
  make_label(box, "1-Wire read = bring-up (GPIO bit-bang)", &lv_font_unscii_8, C_MUTE);
}

static void tool_transit(lv_obj_t *box) {        // RFID/NFC > Transit card
  lv_obj_t *p = panel(box);
  make_label(p, "TRANSIT CARD", &lv_font_unscii_8, C_GREEN);
  if (!nfc_present()) {
    char r[16]; fmt_rupees(24550, r, sizeof r);
    make_label(p, "demo - PN532 not detected", &lv_font_montserrat_16, C_AMBER);
    char line[40]; snprintf(line, sizeof line, "Delhi Metro  %s", r);
    make_label(p, line, &lv_font_montserrat_14, C_SUB);
    return;
  }
  uint8_t uid[7], len = 0;
  make_label(p, nfc_read_uid(uid, &len) ? "card present - reading balance..."
                                        : "tap a metro card",
             &lv_font_montserrat_16, C_TXT);
  make_label(box, "balance block is card-specific (bring-up)", &lv_font_unscii_8, C_MUTE);
}

static void tool_ndef(lv_obj_t *box) {           // RFID/NFC > Write NDEF tag
  lv_obj_t *p = panel(box);
  const char *url = "https://github.com/kavin-jain";
  uint8_t rec[64];
  size_t n = ndef_uri_record(url, rec, sizeof rec);
  make_label(p, "WRITE NDEF TAG", &lv_font_unscii_8, C_GREEN);
  make_label(p, url, &lv_font_montserrat_14, C_TXT);
  char h[40]; snprintf(h, sizeof h, "NDEF record ready: %u bytes", (unsigned)n);
  make_label(p, h, &lv_font_montserrat_14, C_GREEN_SFT);
  make_label(p, nfc_present() ? "tap an NTAG - click to write"
                              : "demo - PN532 not detected",
             &lv_font_montserrat_14, C_SUB);
}

static void tool_gatt(lv_obj_t *box) {           // Bluetooth > GATT explore
  lv_obj_t *p = panel(box);
  make_label(p, "GATT EXPLORE", &lv_font_unscii_8, C_GREEN);
  // Demo: resolve a few common service UUIDs to names (real connect = bring-up).
  static const uint16_t demo[] = {0x1800, 0x180A, 0x180F, 0x180D};
  for (unsigned k = 0; k < sizeof(demo) / sizeof(demo[0]); k++) {
    char line[48]; snprintf(line, sizeof line, "0x%04X  %s", demo[k],
                            gatt_service_name(demo[k]));
    make_label(p, line, &lv_font_montserrat_14, C_TXT);
  }
  make_label(box, "connect + enumerate a device = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_wof(lv_obj_t *box) {            // Bluetooth > Wall of Flipper
  lv_obj_t *p = panel(box);
  int n = ble_count();
  if (n <= 0) n = ble_scan(3);
  int gear = 0;
  make_label(p, "WALL OF FLIPPER", &lv_font_unscii_8, C_CYAN);
  for (int i = 0; i < n; i++) {
    const char *g = wof_identify(ble_name(i));
    if (!g) continue;
    gear++;
    char line[64]; snprintf(line, sizeof line, "%s  %d dBm", g, ble_rssi(i));
    make_label(p, line, &lv_font_montserrat_14, C_AMBER);
  }
  if (gear == 0)
    make_label(p, n > 0 ? "no hacking gear nearby" : "scanning...",
               &lv_font_montserrat_16, C_GREEN_SFT);
  make_label(box, "spots Flippers / pwnagotchis / Marauders", &lv_font_unscii_8, C_MUTE);
}

static void tool_tracker_hunt(lv_obj_t *box) {   // Bluetooth > Tracker hunt
  lv_obj_t *p = panel(box);
  const uint8_t demo[] = {0x4C, 0x00, 0x12, 0x19};   // Apple AirTag adv data
  const char *brand = ble_tracker_brand(ble_company_id(demo, sizeof demo));
  make_label(p, "TRACKER HUNT", &lv_font_unscii_8, C_CYAN);
  make_label(p, "find AirTag / Tile / SmartTag", &lv_font_montserrat_16, C_TXT);
  char h[40]; snprintf(h, sizeof h, "nearest: %s", brand ? brand : "none");
  make_label(p, h, &lv_font_montserrat_20, C_GREEN_SFT);
  make_label(p, "walk around - RSSI rises as you near it",
             &lv_font_montserrat_14, C_SUB);
  make_label(box, "BLE mfg-data scan = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_tracker(lv_obj_t *box) {        // Am I safe? > Tracker on me?
  lv_obj_t *p = panel(box);
  int n = ble_count();
  if (n <= 0) n = ble_scan(3);
  int trackers = 0;
  for (int i = 0; i < n; i++) if (ble_is_tracker(i)) trackers++;
  make_label(p, "BUG SWEEP", &lv_font_unscii_8, C_CYAN);
  if (trackers > 0) {
    char h[32]; snprintf(h, sizeof h, "%d tracker(s) near you", trackers);
    make_label(p, h, &lv_font_montserrat_16, C_AMBER);
    for (int i = 0; i < n; i++) {
      if (!ble_is_tracker(i)) continue;
      const char *nm = ble_name(i);
      if (!nm[0]) nm = ble_addr(i);
      char line[64]; snprintf(line, sizeof line, "%s  %d dBm", nm, ble_rssi(i));
      make_label(p, line, &lv_font_montserrat_14, C_AMBER);
    }
  } else {
    make_label(p, n > 0 ? "No trackers following you" : "scanning...",
               &lv_font_montserrat_16, C_GREEN_SFT);
  }
  make_label(box, "BLE sweep - camera/audio sweep next", &lv_font_montserrat_14, C_SUB);
}

static void tool_generic(lv_obj_t *box, const Tool &t) {
  lv_obj_t *p = panel(box);
  make_label(p, t.name, &lv_font_montserrat_20, C_TXT);
  make_label(p, t.sub, &lv_font_montserrat_14, C_SUB);
  uint32_t fg, bg; kind_colors(t.kind, fg, bg);
  lv_obj_t *b = make_label(p, kind_text(t.kind), &lv_font_unscii_8, fg);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(b, 4, 0);
  lv_obj_set_style_pad_hor(b, 6, 0); lv_obj_set_style_pad_ver(b, 3, 0);
  make_label(p, "> live data lands next milestone", &lv_font_montserrat_14, C_GREEN_SFT);
}

static void build_tool(int c, int i) {
  const Tool &t = CATS[c].tools[i];
  lv_obj_t *scr = new_screen(CATS[c].name);
  section(scr, t.code);
  lv_obj_t *box = content_box(scr);
  if      (c == 0 && i == 0) tool_freq_finder(box);   // Sub-GHz > Frequency finder
  else if (c == 0 && i == 1) tool_subghz_capture(box);// Sub-GHz > Capture & replay
  else if (c == 0 && i == 5) tool_wmbus(box);         // Sub-GHz > wM-Bus meter
  else if (c == 1 && i == 0) tool_nfc_read(box);      // RFID/NFC > Read / clone
  else if (c == 1 && i == 1) tool_mifare(box);        // RFID/NFC > Mifare crack
  else if (c == 1 && i == 2) tool_emv(box);           // RFID/NFC > Bank card read
  else if (c == 1 && i == 5) tool_amiibo(box);        // RFID/NFC > Amiibo clone
  else if (c == 1 && i == 3) tool_transit(box);       // RFID/NFC > Transit card
  else if (c == 1 && i == 4) tool_ndef(box);          // RFID/NFC > Write NDEF tag
  else if (c == 1 && i == 6) tool_ibutton(box);       // RFID/NFC > iButton key
  else if (c == 2 && i == 0) tool_ir_universal(box);  // IR > Universal remote
  else if (c == 2 && i == 1) tool_ir_learn(box);      // IR > Learn & blast
  else if (c == 2 && i == 2) tool_tvbgone(box);       // IR > TV-B-Gone
  else if (c == 9 && i == 0) tool_tvbgone(box);       // Pranks > TV-B-Gone
  else if (c == 9 && i == 2) tool_hackscreen(box);    // Pranks > Hacker screen
  else if (c == 9 && i == 4) tool_castcrash(box);     // Pranks > Cast crasher
  else if (c == 3 && i == 0) tool_wifi_scan(box);     // WiFi > Scan / recon
  else if (c == 3 && i == 1) tool_deauth_atk(box);    // WiFi > Deauth (authorized)
  else if (c == 3 && i == 2) tool_evilportal(box);    // WiFi > Evil Portal
  else if (c == 3 && i == 3) tool_handshake(box);     // WiFi > Handshake / PMKID
  else if (c == 3 && i == 4) tool_wardrive(box);      // WiFi > Wardrive
  else if (c == 3 && i == 5) tool_karma(box);         // WiFi > Karma / MANA
  else if (c == 4 && i == 0) tool_ble_scan(box);      // Bluetooth > Scan / recon
  else if (c == 4 && i == 1) tool_gatt(box);          // Bluetooth > GATT explore
  else if (c == 4 && i == 2) tool_tracker_hunt(box);  // Bluetooth > Tracker hunt
  else if (c == 4 && i == 3) tool_wof(box);           // Bluetooth > Wall of Flipper
  else if (c == 5 && i == 0) tool_mousejack(box);     // NRF24 / 2.4GHz > Mousejack
  else if (c == 5 && i == 1) tool_keysniff(box);      // NRF24 / 2.4GHz > Keyboard sniff
  else if (c == 5 && i == 2) tool_nrf_scan(box);      // NRF24 / 2.4GHz > Band scanner
  else if (c == 8 && i == 0) tool_csi(box);           // See invisible > See through wall
  else if (c == 8 && i == 1) tool_df(box);            // See invisible > Direction finder
  else if (c == 6 && i == 0) tool_badusb(box);        // BadUSB / HID > DuckyScript
  else if (c == 10 && i == 0) tool_buspirate(box);    // Tools / Bench > Bus Pirate
  else if (c == 10 && i == 1) tool_fwdump(box);       // Tools / Bench > Firmware dump
  else if (c == 10 && i == 2) tool_gpio(box);         // Tools / Bench > GPIO play
  else if (c == 11 && i == 0) tool_espnow(box);       // Comms / Off-grid > ESP-NOW mesh
  else if (c == 12 && i == 0) tool_usage(box);        // Me > Claude usage
  else if (c == 12 && i == 1) tool_calendar(box);     // Me > Calendar
  else if (c == 12 && i == 2) tool_tasks(box);        // Me > Tasks
  else if (c == 7 && i == 0) tool_camera(box);        // Am I safe? > Hidden camera
  else if (c == 7 && i == 1) tool_tracker(box);       // Am I safe? > Tracker on me?
  else if (c == 7 && i == 3) tool_deauth(box);        // Am I safe? > Deauth detector
  else if (c == 7 && i == 2) tool_audiobug(box);      // Am I safe? > Audio bug sweep
  else if (c == 7 && i == 4) tool_skimmer(box);       // Am I safe? > Skimmer detector
  else if (c == 7 && i == 5) tool_droneid(box);       // Am I safe? > Drone spotter
  else                       tool_generic(box, t);
  load_screen(scr);
}

static void build_edit_bright() {
  lv_obj_t *scr = new_screen("BRIGHTNESS");
  section(scr, "ROTATE TO CHANGE - BACK TO SAVE");
  lv_obj_t *box = content_box(scr);
  lv_obj_t *p = panel(box);
  g_edit_label = make_label(p, "", &lv_font_montserrat_28, C_GREEN);
  lv_label_set_text_fmt(g_edit_label, "%d%%", g_bright_pct);
  make_label(p, "screen backlight", &lv_font_montserrat_14, C_SUB);
  make_label(box, "needs the BL mod to take effect", &lv_font_unscii_8, C_MUTE);
  load_screen(scr);
  g_edit_val = &g_bright_pct;                    // enable edit mode (render_top cleared it)
  g_edit_min = 10; g_edit_max = 100; g_edit_step = 10;
  g_edit_cb = apply_brightness;
}

static void build_settings() {
  lv_obj_t *scr = new_screen("SETTINGS");
  lv_obj_t *list = make_list(scr);
  char buf[40];
  snprintf(buf, sizeof(buf), "%d %%  (rotate to change)", g_bright_pct);
  add_row(list, "BRT", C_GREEN, "Brightness", buf, NULL, 0, 0, nav_code(SCR_EDIT_BRIGHT, 0, 0));
  snprintf(buf, sizeof(buf), "dim %ds  sleep %ds", DIM_AFTER_MS / 1000, SLEEP_AFTER_MS / 1000);
  add_row(list, "PWR", C_GREEN, "Sleep timers", buf, NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  add_row(list, "THM", C_GREEN, "Theme", "phosphor green", NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  if (storage_ready())
    snprintf(buf, sizeof(buf), "SD %lu / %lu MB used", (unsigned long)storage_used_mb(),
             (unsigned long)storage_total_mb());
  else
    snprintf(buf, sizeof(buf), "no card - insert to save");
  add_row(list, "SD", storage_ready() ? C_GREEN : C_SUB, "Storage", buf, NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  add_row(list, "?",   C_SUB,   "About", "Edgehax S3-PRO  -  fw m2", NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  load_screen(scr);
}

// ---------------------------------------------------------------- nav engine
static void render_top() {
  g_edit_val = nullptr;                           // leaving any screen exits edit mode
  g_edit_label = nullptr;
  NavEntry &e = nav_stack[nav_depth - 1];
  switch (e.t) {
    case SCR_HOME:        build_home();               break;
    case SCR_AROUND:      build_around();             break;
    case SCR_CATEGORY:    build_category(e.cat);      break;
    case SCR_TOOL:        build_tool(e.cat, e.tool);  break;
    case SCR_SETTINGS:    build_settings();           break;
    case SCR_EDIT_BRIGHT: build_edit_bright();        break;
  }
}
static void nav_push(ScreenT t, int cat, int tool) {
  if (nav_depth >= 8) return;
  nav_stack[nav_depth++] = { t, (int8_t)cat, (int8_t)tool };
  render_top();
}
static void nav_pop() {
  if (nav_depth <= 1) return;
  nav_depth--;
  render_top();
}
static void nav_home() {
  nav_depth = 1;
  nav_stack[0] = { SCR_HOME, 0, 0 };
  render_top();
}
static void on_action() {                          // context key — reserved
  Serial.println("[ui] ACTION");
}

// ---------------------------------------------------------------- styles
static void init_styles() {
  lv_style_init(&st_screen);
  lv_style_set_bg_color(&st_screen, lv_color_hex(C_BG));
  lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);
  lv_style_set_pad_all(&st_screen, 0);
  lv_style_set_border_width(&st_screen, 0);

  lv_style_init(&st_item);
  lv_style_set_bg_color(&st_item, lv_color_hex(C_CARD));
  lv_style_set_bg_opa(&st_item, LV_OPA_COVER);
  lv_style_set_border_color(&st_item, lv_color_hex(C_LINE));
  lv_style_set_border_width(&st_item, 1);
  lv_style_set_radius(&st_item, 9);

  lv_style_init(&st_item_foc);
  lv_style_set_bg_color(&st_item_foc, lv_color_hex(C_CARD_FOC));
  lv_style_set_border_color(&st_item_foc, lv_color_hex(C_GREEN));
  lv_style_set_border_width(&st_item_foc, 2);

  lv_style_init(&st_chip);
  lv_style_set_bg_color(&st_chip, lv_color_hex(C_CHIP));
  lv_style_set_bg_opa(&st_chip, LV_OPA_COVER);
  lv_style_set_radius(&st_chip, 7);
  lv_style_set_border_width(&st_chip, 0);
  lv_style_set_pad_all(&st_chip, 0);
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[ui] boot — UI shell m2");

  // backlight PWM (no-op until the LED pin is rewired off 3V3 to PIN_BL_PWM)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BL_PWM, BL_LEDC_FREQ, BL_LEDC_BITS);
#else
  ledcSetup(BL_LEDC_CH, BL_LEDC_FREQ, BL_LEDC_BITS);
  ledcAttachPin(PIN_BL_PWM, BL_LEDC_CH);
#endif
  bl_write(bl_user_duty);

  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);

  // SD shares SPI-A with the TFT — mount after the display bus is up.
  storage_begin();
  Serial.printf("[sd] %s (%lu/%lu MB)\n", storage_ready() ? "mounted" : "no card",
                (unsigned long)storage_used_mb(), (unsigned long)storage_total_mb());

  cc1101_begin();
  Serial.printf("[cc1101] %s (ver 0x%02x)\n", cc1101_present() ? "present" : "absent",
                cc1101_present() ? cc1101_version() : 0);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCR_W * 40);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCR_W;
  disp_drv.ver_res = SCR_H;
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  init_styles();

  // encoder — interrupts + quadrature state seed
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  enc_prev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), enc_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), enc_isr, CHANGE);

  // expander — buttons + encoder click (graceful if not wired yet)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  mcp_ok = mcp.begin_I2C(MCP_ADDR, &Wire);
  if (mcp_ok) {
    for (uint8_t p = 0; p <= MCP_ENC_SW; p++) mcp.pinMode(p, INPUT_PULLUP);
    mcp.setupInterrupts(true, false, LOW);      // mirror, push-pull, active-low
    for (uint8_t p = 0; p <= MCP_ENC_SW; p++) mcp.setupInterruptPin(p, CHANGE);
    pinMode(PIN_MCP_INT, INPUT_PULLUP);
    Serial.println("[ui] MCP23017 ok — buttons live");
  } else {
    Serial.println("[ui] MCP23017 NOT found — encoder rotate only until wired");
  }

  // PN532 NFC (shares the I2C bus started above)
  nfc_begin();
  Serial.printf("[nfc] PN532 %s\n", nfc_present() ? "present" : "absent");

  ir_begin();   // IR TX/RX (plain GPIO, always ready)

  // LVGL encoder input device + focus group
  g_group = lv_group_create();
  static lv_indev_drv_t enc_drv;
  lv_indev_drv_init(&enc_drv);
  enc_drv.type = LV_INDEV_TYPE_ENCODER;
  enc_drv.read_cb = enc_read_cb;
  g_enc_indev = lv_indev_drv_register(&enc_drv);
  lv_indev_set_group(g_enc_indev, g_group);

  lv_timer_create(poll_buttons, 30, NULL);      // button poll (also updates click)

  last_input_ms = millis();
  nav_home();                                   // build the first screen
  Serial.println("[ui] home ready");
}

void loop() {
  lv_timer_handler();
  pm_tick();
  delay(5);
}
