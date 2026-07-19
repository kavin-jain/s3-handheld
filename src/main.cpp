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
#include "nfc_pn532.h"
#include "nfc_keys.h"
#include "ir_remote.h"
#include "wifi_scan.h"
#include "wifi_fmt.h"
#include "ble_scan.h"
#include "badusb.h"
#include "deauth_detect.h"

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
enum ScreenT : uint8_t { SCR_HOME, SCR_AROUND, SCR_CATEGORY, SCR_TOOL, SCR_SETTINGS };
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
  data->enc_diff = steps;
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
  else if (c == 1 && i == 0) tool_nfc_read(box);      // RFID/NFC > Read / clone
  else if (c == 2 && i == 1) tool_ir_learn(box);      // IR > Learn & blast
  else if (c == 3 && i == 0) tool_wifi_scan(box);     // WiFi > Scan / recon
  else if (c == 4 && i == 0) tool_ble_scan(box);      // Bluetooth > Scan / recon
  else if (c == 6 && i == 0) tool_badusb(box);        // BadUSB / HID > DuckyScript
  else if (c == 7 && i == 1) tool_tracker(box);       // Am I safe? > Tracker on me?
  else if (c == 7 && i == 3) tool_deauth(box);        // Am I safe? > Deauth detector
  else                       tool_generic(box, t);
  load_screen(scr);
}

static void build_settings() {
  lv_obj_t *scr = new_screen("SETTINGS");
  lv_obj_t *list = make_list(scr);
  char buf[40];
  snprintf(buf, sizeof(buf), "%d %%  (needs BL mod)", (bl_user_duty * 100) / 255);
  add_row(list, "BRT", C_GREEN, "Brightness", buf, NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
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
  NavEntry &e = nav_stack[nav_depth - 1];
  switch (e.t) {
    case SCR_HOME:     build_home();               break;
    case SCR_AROUND:   build_around();             break;
    case SCR_CATEGORY: build_category(e.cat);      break;
    case SCR_TOOL:     build_tool(e.cat, e.tool);  break;
    case SCR_SETTINGS: build_settings();           break;
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
