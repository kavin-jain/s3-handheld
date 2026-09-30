// ESP32-S3 handheld — UI shell, milestone 2.
// Death Note pixel-art theme (charcoal/slate/off-white), rotary-encoder +
// 3-button navigation, a Flipper-style menu tree, PIN lock screen, mascot
// reactions (L/Light/Ryuk/Misa via ui_mascot.h), and a real idle power
// manager (dim -> light sleep -> lock on wake).
// Radio/NFC/etc. data is still static demo — live data is a later milestone.
//
// Controls:  rotate = move  ·  click = select  ·  BACK/HOME/ACTION buttons.
// Renders through TFT_eSPI (confirmed working). Pins come from include/pins.h.

#include <Arduino.h>
#include <string.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "pins.h"
#include "storage.h"
#include "config.h"
#include "power_ctl.h"
#include "power_level.h"
#include "radio_cc1101.h"
#include "subghz_classify.h"
#include "subghz_replay.h"
#include "pt2262.h"
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
#include "ac_db.h"
#include "ac_state.h"
#include "wifi_scan.h"
#include "wifi_fmt.h"
#include "ble_scan.h"
#include "ble_track.h"
#include "gatt_uuid.h"
#include "badusb.h"
#include "hid_encode.h"
#include "gags.h"
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
#include "usbhost.h"

// ---------------------------------------------------------------- power knobs
#define DIM_AFTER_MS     20000    // active -> dim
#define SLEEP_AFTER_MS   35000    // active -> sleep (dim + this gap)
#define DIM_DUTY         40       // backlight duty in DIM (0..255)
#define ENABLE_LIGHT_SLEEP 1      // 0 while USB-debugging (light sleep drops CDC)
#define ENC_STEPS_PER_DETENT 2    // EC11 quadrature transitions per click; tune

// ---------------------------------------------------------------- palette (RGB)
// Death Note theme: charcoal/slate chrome, off-white "ink" accent — see
// sprites.h for the matching character-art palette (INK/PAPER/SLATE/SLATE_D).
// K_ATK/K_DEF/warning stay distinct hues on purpose — the badge system reads
// by colour, not just text.
#define C_BG        0x14181b
#define C_CARD      0x232a2f
#define C_CARD_FOC  0x30393f
#define C_LINE      0x3a444b
#define C_CHIP      0x2c363c
#define C_ACCENT     0xEDE7DD   // ink accent — titles, focus ring, "ready" chips
#define C_ACCENT_SFT 0xB9AE98   // soft/parchment sub-accent
#define C_TXT       0xEDE7DD
#define C_SUB       0x9aa3a8
#define C_MUTE      0x5b656b
#define C_RED       0xd9534f
#define C_RED_BG    0x2a1414
#define C_AMBER     0xd9a441
#define C_CYAN      0x6fb3c2
#define C_CYAN_BG   0x142428

// ---------------------------------------------------------------- display glue
#include "bus_locks.h"
#include "ui_anim.h"
#include "ui_mascot.h"
#include "ui_lock.h"


static TFT_eSPI tft = TFT_eSPI();
static const uint16_t SCR_W = 320, SCR_H = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCR_W * 40];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  uint32_t w = area->x2 - area->x1 + 1, h = area->y2 - area->y1 + 1;
  if (spi_a_mutex) xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&px->full, w * h, true);
  tft.endWrite();
  if (spi_a_mutex) xSemaphoreGiveRecursive(spi_a_mutex);
  lv_disp_flush_ready(drv);
}

// ---------------------------------------------------------------- feature data
enum Kind : uint8_t { K_OK, K_SOON, K_ATK, K_DEF };   // badge flavor

struct Tool {
  const char *code; const char *name; const char *sub; Kind kind;
  void (*render)(lv_obj_t*);
};
struct Category { const char *icon; const char *name; const char *tag; const Tool *tools; uint8_t n; };

// Forward declarations for tools
static void tool_freq_finder(lv_obj_t*);
static void tool_subghz_capture(lv_obj_t*);
static void tool_ism(lv_obj_t*);
static void tool_wmbus(lv_obj_t*);
static void tool_nfc_read(lv_obj_t*);
static void tool_mifare(lv_obj_t*);
static void tool_emv(lv_obj_t*);
static void tool_amiibo(lv_obj_t*);
static void tool_transit(lv_obj_t*);
static void tool_ndef(lv_obj_t*);
static void tool_ibutton(lv_obj_t*);
static void tool_ir_universal(lv_obj_t*);
static void tool_ir_learn(lv_obj_t*);
static void tool_tvbgone(lv_obj_t*);
static void tool_wifi_scan(lv_obj_t*);
static void tool_deauth_atk(lv_obj_t*);
static void tool_evilportal(lv_obj_t*);
static void tool_handshake(lv_obj_t*);
static void tool_wardrive(lv_obj_t*);
static void tool_karma(lv_obj_t*);
static void tool_ble_scan(lv_obj_t*);
static void tool_gatt(lv_obj_t*);
static void tool_tracker_hunt(lv_obj_t*);
static void tool_wip(lv_obj_t*);
static void tool_mousejack(lv_obj_t*);
static void tool_kbdsniff(lv_obj_t*);
static void tool_bandscan(lv_obj_t*);
static void tool_ducky(lv_obj_t*);
static void tool_hid(lv_obj_t*);
static void tool_camera(lv_obj_t*);
static void tool_tracker(lv_obj_t*);
static void tool_csi(lv_obj_t*);
static void tool_dirfind(lv_obj_t*);
static void tool_rickroll(lv_obj_t*);
static void tool_hackscreen(lv_obj_t*);
static void tool_usbgag(lv_obj_t*);
static void tool_castcrash(lv_obj_t*);
static void tool_buspirate(lv_obj_t*);
static void tool_fwdump(lv_obj_t*);
static void tool_gpio(lv_obj_t*);
static void tool_espnow(lv_obj_t*);
static void tool_usbhost(lv_obj_t*);
static void tool_fido(lv_obj_t*);
static void tool_df(lv_obj_t*);
static void tool_nrf_scan(lv_obj_t*);
static void tool_audiobug(lv_obj_t*);
static void tool_skimmer(lv_obj_t*);
static void tool_calendar(lv_obj_t*);
static void tool_tasks(lv_obj_t*);
static void tool_wof(lv_obj_t*);
static void tool_keysniff(lv_obj_t*);
static void tool_deauth(lv_obj_t*);
static void tool_droneid(lv_obj_t*);
static void tool_badusb(lv_obj_t*);
static void tool_hidattack(lv_obj_t*);

static const Tool T_SUBGHZ[] = {
  {"SG",  "Frequency finder", "sweep the band, name the signal", K_OK, tool_freq_finder},
  {"CR",  "Capture & replay", "fixed-code gates & remotes",      K_ATK, tool_subghz_capture},
  {"BF",  "Gate brute-force", "De Bruijn - your own gate",       K_ATK, nullptr},
  {"TS",  "Tesla charge port","315 MHz open",                    K_OK, nullptr},
  {"433", "ISM decoder",      "weather/doorbell/TPMS (rtl_433)", K_OK, tool_ism},
  {"WMB", "wM-Bus meter",     "read utility meters, 868 MHz",    K_OK, tool_wmbus},
};
static const Tool T_NFC[] = {
  {"NR",  "Read / clone",     "Mifare, NTAG & more",             K_OK, tool_nfc_read},
  {"MF",  "Mifare crack",     "dictionary keys",                 K_ATK, tool_mifare},
  {"EMV", "Bank card read",   "public data only",                K_OK, tool_emv},
  {"TR",  "Transit card",     "metro balance & history",         K_OK, tool_transit},
  {"ND",  "Write NDEF tag",   "URL / WiFi / vCard",              K_OK, tool_ndef},
  {"AM",  "Amiibo clone",     "to NTAG215",                      K_OK, tool_amiibo},
  {"IB",  "iButton key",      "1-Wire Dallas",                   K_OK, tool_ibutton},
};
static const Tool T_IR[] = {
  {"UR",  "Universal remote", "TV + A/C brand database",         K_OK, tool_ir_universal},
  {"LB",  "Learn & blast",    "capture any remote",              K_OK, tool_ir_learn},
  {"TVB", "TV-B-Gone",        "shut off any TV",                 K_OK, tool_tvbgone},
};
static const Tool T_WIFI[] = {
  {"SC",  "Scan / recon",     "who is here",                     K_OK, tool_wifi_scan},
  {"DA",  "Deauth",           "kick a client",                   K_ATK, tool_deauth_atk},
  {"EP",  "Evil Portal",      "captive login clone",             K_ATK, tool_evilportal},
  {"HS",  "Handshake / PMKID","capture to SD",                   K_ATK, tool_handshake},
  {"WD",  "Wardrive",         "log nets + GPS to SD",            K_OK, tool_wardrive},
  {"KM",  "Karma / MANA",     "auto-associate probes",           K_ATK, tool_karma},
};
static const Tool T_BLE[] = {
  {"BS",  "Scan / recon",     "devices around you",              K_OK, tool_ble_scan},
  {"GT",  "GATT explore",     "services & characteristics",      K_OK, tool_gatt},
  {"TK",  "Tracker hunt",     "AirTag / Tile near me",           K_DEF, tool_tracker_hunt},
  {"WF",  "Wall of Flipper",  "spot other hacking gear",         K_DEF, tool_wof},
};
static const Tool T_NRF[] = {
  {"MJ",  "Mousejack",        "wireless kbd/mouse inject",       K_ATK, tool_mousejack},
  {"KS",  "Keyboard sniff",   "log 2.4 GHz keystrokes",          K_ATK, tool_keysniff},
  {"BN",  "Band scanner",     "2.4 GHz activity map",            K_OK, tool_nrf_scan},
};
static const Tool T_USB[] = {
  {"DK",  "DuckyScript",      "run payload from SD",             K_ATK, tool_badusb},
  {"HID", "HID attacks",      "keystroke injection",             K_ATK, tool_hidattack},
};
static const Tool T_SPY[] = {
  {"HC",  "Hidden camera",    "wireless lens finder",            K_DEF, tool_camera},
  {"ME",  "Tracker on me?",   "GPS/BLE bug sweep",               K_DEF, tool_tracker},
  {"AB",  "Audio bug sweep",  "RF listening devices",            K_DEF, tool_audiobug},
  {"DD",  "Deauth detector",  "is someone jamming me?",          K_DEF, tool_deauth},
  {"SK",  "Skimmer detector", "rogue card readers",              K_DEF, tool_skimmer},
  {"DR",  "Drone spotter",    "Remote-ID + pilot location",      K_DEF, tool_droneid},
};
static const Tool T_SENSE[] = {
  {"WW",  "See through wall",  "WiFi CSI motion & breathing",    K_OK, tool_csi},
  {"DF",  "Direction finder",  "fox-hunt a signal",             K_OK, tool_df},
};
static const Tool T_FUN[] = {
  {"TVB", "TV-B-Gone",        "turn everything off",             K_OK, tool_tvbgone},
  {"RR",  "Rickroll tag",     "NFC that opens the song",         K_OK, tool_rickroll},
  {"HK",  "Hacker screen",    "fake spy-movie hack",             K_OK, tool_hackscreen},
  {"GG",  "USB gag",          "harmless keyboard prank",         K_OK, tool_usbgag},
  {"CST", "Cast crasher",     "poke nearby TVs / Rokus",         K_OK, tool_castcrash},
};
static const Tool T_TOOLS[] = {
  {"BP",  "Bus Pirate",       "sniff I2C/SPI/UART/JTAG",         K_OK, tool_buspirate},
  {"FD",  "Firmware dump",    "read & analyse flash",            K_OK, tool_fwdump},
  {"GP",  "GPIO play",        "toggle & read pins",              K_OK, tool_gpio},
};
static const Tool T_COMMS[] = {
  {"EN",  "ESP-NOW mesh",     "router-free messaging",           K_OK, tool_espnow},
  {"UH",  "USB host",         "read a flash drive",              K_OK, tool_usbhost},
  {"MT",  "Meshtastic",       "needs LoRa add-on",               K_SOON, nullptr},
};
static const Tool T_ME[] = {
  {"CL",  "Claude usage",     "5h & weekly meter",               K_SOON, nullptr},
  {"CAL", "Calendar",         "from your phone",                 K_OK, tool_calendar},
  {"TSK", "Tasks",            "from your phone",                 K_OK, tool_tasks},
  {"FID", "Security key",     "FIDO2 / U2F",                     K_OK, nullptr},
  {"LNK", "Phone + web",      "companion & dashboard",           K_SOON, nullptr},
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
enum ScreenT : uint8_t { SCR_HOME, SCR_AROUND, SCR_CATEGORY, SCR_TOOL, SCR_SETTINGS,
                         SCR_EDIT_BRIGHT, SCR_EDIT_POWER, SCR_SPLASH, SCR_LOCK, SCR_SET_PIN };
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
static bool     g_wake_needs_lock = false;     // set right before light-sleep; consumed by pm_wake()

// ---------------------------------------------------------------- lock screen
static LockState g_lock = {-1, {0, 0, 0, 0}, 0};   // pin=-1: no PIN configured
static bool      g_locked = false;                 // gates poll_buttons() below
static bool      g_lock_setting_new = false;        // SCR_LOCK (false) vs SCR_SET_PIN (true)
static lv_obj_t *g_lock_dots[4] = {nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *g_lock_digit_lbl = nullptr;
static lv_obj_t *g_lock_status_lbl = nullptr;
static int       g_lock_digit = 0;                  // 0-9, driven by the encoder edit-mode

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
static int  g_power_lvl = PWR_MAX;              // global intensity (scales radio TX power)
static int *g_edit_val = nullptr;
static int  g_edit_min, g_edit_max, g_edit_step;
static void (*g_edit_cb)(int) = nullptr;
static lv_obj_t *g_edit_label = nullptr;
// Per-screen ACTION-button handler (e.g. "save this capture to SD"). Cleared on
// every screen change; invoked by on_action().
static void (*g_action_cb)() = nullptr;
// IR universal-remote live brand selector (encoder cycles g_ir_brand).
static int       g_ir_brand = 0;
static int       g_ac_temp = 24;               // A/C setpoint (ACTION bumps it)
static lv_obj_t *g_ir_name_lbl = nullptr;
static lv_obj_t *g_ir_code_lbl = nullptr;
// Sub-GHz frequency finder live band selector (encoder cycles g_freq_idx).
static const float FREQ_PRESETS[] = {300.0f, 315.0f, 390.0f, 433.92f, 868.0f, 915.0f};
static const int   FREQ_N = sizeof(FREQ_PRESETS) / sizeof(FREQ_PRESETS[0]);
static int       g_freq_idx = 3;               // default 433.92 MHz
// Pranks USB-gag live selector (encoder cycles g_gag through the tested GAGS lib).
static int       g_gag = 0;
static lv_obj_t *g_gag_name = nullptr;
static lv_obj_t *g_gag_line = nullptr;
static lv_obj_t *g_gag_status = nullptr;
static lv_obj_t *g_freq_mhz = nullptr, *g_freq_sub = nullptr;
static lv_obj_t *g_freq_guess = nullptr, *g_freq_bar = nullptr;

static void apply_brightness(int pct) {
  bl_user_duty = (uint8_t)(pct * 255 / 100);
  bl_write(bl_user_duty);
}

// Settings persistence: serialise current prefs to /config.txt (see config.h).
static bool g_cfg_dirty = false;
static void save_config_now() {
  DeviceCfg cfg = {g_bright_pct, DIM_AFTER_MS / 1000, SLEEP_AFTER_MS / 1000, 0, g_power_lvl, g_lock.pin};
  char line[72];
  if (cfg_serialize(&cfg, line, sizeof line)) storage_save_config(line);
}
// Brightness edit callback: apply live, update its label, mark dirty.
static void bright_edit_cb(int pct) {
  apply_brightness(pct);
  if (g_edit_label) lv_label_set_text_fmt(g_edit_label, "%d%%", pct);
  g_cfg_dirty = true;
}

// Global intensity dial (Low/Med/Max) — scales every radio's TX power.
static void power_edit_cb(int lvl) {
  set_power_level(lvl);                           // radios read this on next use
  if (g_edit_label) lv_label_set_text(g_edit_label, pwr_name(lvl));
  g_cfg_dirty = true;
  if (lvl == PWR_MAX) mascot_play(lv_scr_act(), MASCOT_RYUK, LV_ALIGN_TOP_RIGHT, false);
}

static void nav_lock();
static void pm_wake() {                        // -> ACTIVE (called on any input)
  last_input_ms = millis();
  if (pm_state != PM_ACTIVE) {
    setCpuFrequencyMhz(240);
    bl_write(bl_user_duty);
    pm_state = PM_ACTIVE;
  }
  if (g_wake_needs_lock) {                      // woke from an actual light-sleep
    g_wake_needs_lock = false;
    if (lock_configured(&g_lock)) nav_lock();
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
    g_wake_needs_lock = true;
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
// A plain time-gated debounce wasn't enough on this board: the shared GPIO ISR
// dispatcher (gpio_isr_loop) keeps re-entering as long as the line keeps
// toggling, regardless of how fast the handler body returns, and that alone
// starved the watchdog. So mask the interrupt at the peripheral instead --
// gpio_intr_disable() stops it from firing again at all until enc_rearm_tick()
// (called from loop(), not ISR context) re-enables it after the line settles.
static const int8_t QDEC[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
static volatile bool     enc_masked = false;
static volatile uint32_t enc_mask_us = 0;
#define ENC_REARM_US 3000   // gap before trusting the line again after a storm trip
static void IRAM_ATTR enc_isr() {
  gpio_intr_disable((gpio_num_t)PIN_ENC_A);
  gpio_intr_disable((gpio_num_t)PIN_ENC_B);
  enc_mask_us = micros();
  enc_masked = true;
  uint8_t s = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  enc_accum += QDEC[((enc_prev << 2) | s) & 0x0f];
  enc_prev = s;
}

static void enc_rearm_tick() {
  if (enc_masked && (uint32_t)(micros() - enc_mask_us) >= ENC_REARM_US) {
    enc_masked = false;
    gpio_intr_enable((gpio_num_t)PIN_ENC_A);
    gpio_intr_enable((gpio_num_t)PIN_ENC_B);
  }
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
      if (g_edit_cb) g_edit_cb(nv);              // callback owns the display update
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

static void lock_repaint_dots();               // fwd (defined near build_lock)
static void poll_buttons(lv_timer_t *) {
  if (!mcp_ok) return;
  uint8_t pressed = (~mcp.readGPIOA()) & 0x0f;   // 1 = pressed
  uint8_t edges   = pressed & ~btn_prev;         // rising (newly pressed)
  btn_prev = pressed;
  g_enc_pressed = pressed & (1 << MCP_ENC_SW);
  if (edges) pm_wake();
  if (g_locked) {                                // lock screen owns BACK/HOME;
    if (edges & ((1 << MCP_BTN_BACK) | (1 << MCP_BTN_HOME))) {
      lock_reset_entry(&g_lock);
      lock_repaint_dots();
    } else if (edges & (1 << MCP_BTN_ACTION)) {
      on_action();                               // ACTION still confirms a digit
    }
    return;                                      // never falls through to nav
  }
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
    default:     fg = C_ACCENT; bg = C_CHIP;    break;
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
  make_label(bar, title, &lv_font_unscii_8, C_ACCENT);
  lv_obj_t *sp = plain(bar); lv_obj_set_flex_grow(sp, 1); lv_obj_set_height(sp, 1);
  make_label(bar, LV_SYMBOL_GPS, &lv_font_montserrat_14, C_MUTE);
  make_label(bar, LV_SYMBOL_SD_CARD, &lv_font_montserrat_14, storage_ready() ? C_ACCENT : C_MUTE);
  make_label(bar, LV_SYMBOL_BATTERY_FULL " 82%", &lv_font_unscii_8, C_ACCENT_SFT);
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
static void build_edit_power();
static void build_splash();
static void build_lock(bool setting_new);

static lv_timer_t *g_tool_timer = nullptr;
static void (*g_cleanup_cb)() = nullptr;

static lv_obj_t *new_screen(const char *title) {
  if (g_tool_timer) { lv_timer_del(g_tool_timer); g_tool_timer = nullptr; }
  if (g_cleanup_cb) { g_cleanup_cb(); g_cleanup_cb = nullptr; }
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
  add_row(list, "\xE2\x97\x89", C_ACCENT, "Around me", "live radar of what's near you",
          "live", C_ACCENT, C_CHIP, nav_code(SCR_AROUND, 0, 0));
  for (uint8_t i = 0; i < N_CATS; i++)
    add_row(list, CATS[i].icon, C_ACCENT, CATS[i].name, CATS[i].tag,
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
  around_card(list, "RF",  C_ACCENT, "Gate remote",   "433.92 MHz  fixed code",   "copy",    C_ACCENT,C_CHIP);
  around_card(list, "IR",  C_AMBER, "Samsung TV",    "infrared  ready",          "control", C_ACCENT,C_CHIP);
  around_card(list, "WiFi",C_ACCENT, "5 nets 9 devices","tap to see who is here", "explore", C_ACCENT,C_CHIP);
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

// Repaint MHz / RSSI / bar / guess for the selected band (encoder edit callback).
static void freq_paint(int idx) {
  if (idx < 0 || idx >= FREQ_N) return;
  float f = FREQ_PRESETS[idx];
  int rssi = cc1101_present() ? cc1101_rssi_at(f) : -70;   // demo RSSI w/o radio
  if (g_freq_mhz)   lv_label_set_text_fmt(g_freq_mhz, "%.2f", f);
  if (g_freq_sub)   lv_label_set_text_fmt(g_freq_sub, "MHz  -  %d dBm", rssi);
  if (g_freq_bar)   lv_bar_set_value(g_freq_bar, sg_bar_pct(rssi), LV_ANIM_OFF);
  if (g_freq_guess) lv_label_set_text(g_freq_guess, sg_guess(f));
}

static void tool_freq_finder(lv_obj_t *box) {   // Sub-GHz > Frequency finder
  if (g_freq_idx >= FREQ_N) g_freq_idx = 3;
  bool live = cc1101_present();
  lv_obj_t *p = panel(box);
  g_freq_mhz = make_label(p, "", &lv_font_montserrat_28, C_ACCENT);
  g_freq_sub = make_label(p, "", &lv_font_unscii_8, C_SUB);
  g_freq_bar = lv_bar_create(p);
  lv_obj_set_size(g_freq_bar, lv_pct(100), 10);
  lv_obj_set_style_bg_color(g_freq_bar, lv_color_hex(C_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_freq_bar, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
  g_freq_guess = make_label(p, "", &lv_font_montserrat_16, C_TXT);
  make_label(p, live ? "rotate to tune the band" : "demo - CC1101 not detected",
             &lv_font_montserrat_14, live ? C_ACCENT_SFT : C_AMBER);
  make_label(box, "rotate = band    click = lock", &lv_font_unscii_8, C_MUTE);
  freq_paint(g_freq_idx);
  // Live-select: encoder cycles the preset band and re-measures in place.
  g_edit_val = &g_freq_idx;
  g_edit_min = 0; g_edit_max = FREQ_N - 1; g_edit_step = 1;
  g_edit_cb = freq_paint;
}

// Last sub-GHz capture + its on-screen save status (ACTION button writes to SD).
static uint32_t g_sub_code = 0;
static uint8_t  g_sub_bits = 0;
static int      g_sub_proto = 0;
static lv_obj_t *g_sub_status = nullptr;

static void subghz_save_action() {
  char body[64];
  int n = snprintf(body, sizeof body, "protocol:P%d\nbits:%u\ncode:0x%06lX\n",
                   g_sub_proto, (unsigned)g_sub_bits, (unsigned long)g_sub_code);
  const char *path = storage_save(SAVE_SUBGHZ, "sub", (const uint8_t *)body, (size_t)n);
  if (g_sub_status)
    lv_label_set_text(g_sub_status, (path && path[0]) ? path : "no SD card");
}

static lv_obj_t *g_sub_panel = nullptr;
static lv_obj_t *g_sub_box = nullptr;

static void subghz_cleanup() {
  subghz_capture_end();
}

static void subghz_poll_cb(lv_timer_t *t) {
  uint32_t code; uint8_t bits; int proto;
  if (subghz_capture_poll(&code, &bits, &proto)) {
    g_cleanup_cb = nullptr; // Clear cleanup so we don't call it again
    subghz_capture_end();
    lv_timer_del(g_tool_timer);
    g_tool_timer = nullptr;
    lv_obj_clean(g_sub_panel);
    
    g_sub_code = code; g_sub_bits = bits; g_sub_proto = proto;
    char h[40]; rcs_fmt(code, bits, proto, h, sizeof h);
    make_label(g_sub_panel, "CAPTURED", &lv_font_unscii_8, C_ACCENT);
    make_label(g_sub_panel, h, &lv_font_montserrat_20, C_TXT);
    make_label(g_sub_panel, "433.92 MHz  -  fixed code (OOK)", &lv_font_montserrat_14, C_SUB);
    g_sub_status = make_label(g_sub_panel, "ACTION = save to /subghz", &lv_font_unscii_8, C_ACCENT_SFT);
    make_label(g_sub_box, "ACTION saves .sub -> SD    click = replay", &lv_font_unscii_8, C_MUTE);
    g_action_cb = subghz_save_action;
    mascot_play(lv_scr_act(), MASCOT_MISA, LV_ALIGN_BOTTOM_MID, false);
  }
}

static void tool_subghz_capture(lv_obj_t *box) { // Sub-GHz > Capture & replay
  lv_obj_t *p = panel(box);
  g_sub_panel = p;
  g_sub_box = box;
  bool live = cc1101_present();
  
  if (!live) {
    make_label(p, "DEMO CAPTURE", &lv_font_unscii_8, C_AMBER);
    make_label(p, "0x0015F3 (24-bit)", &lv_font_montserrat_20, C_TXT);
    make_label(p, "433.92 MHz  -  fixed code (OOK)", &lv_font_montserrat_14, C_SUB);
    g_sub_status = make_label(p, "ACTION = save to /subghz", &lv_font_unscii_8, C_ACCENT_SFT);
    make_label(box, "ACTION saves .sub -> SD    click = replay", &lv_font_unscii_8, C_MUTE);
    g_action_cb = subghz_save_action;
    return;
  }
  
  make_label(p, "LISTENING 433.92", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "press a fob near the antenna", &lv_font_montserrat_16, C_TXT);
  ui_anim_radar_create(p, 60, lv_color_hex(C_CYAN));
  mascot_play(p, MASCOT_L, LV_ALIGN_TOP_RIGHT, true);
  make_label(box, "rotate = band    click = replay", &lv_font_unscii_8, C_MUTE);
  
  g_cleanup_cb = subghz_cleanup;
  subghz_capture_begin(433.92f);
  g_tool_timer = lv_timer_create(subghz_poll_cb, 50, NULL);
}

static void tool_ism(lv_obj_t *box) {            // Sub-GHz > ISM decoder
  lv_obj_t *p = panel(box);
  uint32_t code = 0x00FF0F; int bits = 24;       // demo captured gate code
  char tri[16]; pt2262_tristate(code, bits, tri);
  make_label(p, "433 ISM DECODER", &lv_font_unscii_8, C_ACCENT);
  char h[40]; snprintf(h, sizeof h, "0x%06lX  (%d bit)", (unsigned long)code, bits);
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  char t[24]; snprintf(t, sizeof t, "PT2262: %s", tri);
  make_label(p, t, &lv_font_unscii_8, C_ACCENT_SFT);
  make_label(p, pt2262_is_valid(tri) ? "valid tri-state frame" : "not PT2262",
             &lv_font_montserrat_14, C_SUB);
  make_label(box, "gate/garage remotes - CC1101 RX bring-up",
             &lv_font_unscii_8, C_MUTE);
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
  int sdkeys = 0;
  if (storage_ready()) {                          // + user dictionary from SD
    static char dic[2048];
    if (storage_read_file("/nfc/keys.dic", dic, sizeof dic)) sdkeys = mifare_dict_count(dic);
  }
  char h[48]; snprintf(h, sizeof h, "%d built-in + %d SD keys", MIFARE_KEY_COUNT, sdkeys);
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  char k[48]; snprintf(k, sizeof k, "sec 0 KeyA: %s", mifare_key_name(found));
  make_label(p, k, &lv_font_unscii_8, C_ACCENT_SFT);
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
  make_label(p, ok ? "NTAG215 AMIIBO" : "NOT AMIIBO", &lv_font_unscii_8, C_ACCENT);
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
             &lv_font_montserrat_14, C_ACCENT_SFT);
  make_label(box, "public data only - PN532 APDU = bring-up", &lv_font_unscii_8, C_MUTE);
}

// Last NFC UID (hex) + save status; ACTION writes it to /nfc.
static char      g_nfc_uid[24] = "04:A2:1B:9C";
static lv_obj_t *g_nfc_status = nullptr;
static lv_obj_t *g_nfc_panel = nullptr;
static lv_obj_t *g_nfc_box = nullptr;

static void nfc_save_action() {
  size_t ul = 0; while (g_nfc_uid[ul]) ul++;
  const char *path = storage_save(SAVE_NFC, "nfc", (const uint8_t *)g_nfc_uid, ul);
  if (g_nfc_status)
    lv_label_set_text(g_nfc_status, (path && path[0]) ? path : "no SD card");
}

static void tool_nfc_read(lv_obj_t *box);

// nfc_begin() only ever ran once, in setup() -- if the PN532 handshake lost
// the race at boot, this screen was stuck saying "not detected" until a
// reboot even though the chip answers fine on a live I2C scan. ACTION here
// re-runs the real init and repaints, same pattern as the Bus Pirate rescan.
static void nfc_retry_action() {
  bool ok = nfc_begin();
  Serial.printf("[nfc] retry: PN532 %s\n", ok ? "present" : "absent");
  if (!g_nfc_box) return;
  lv_obj_clean(g_nfc_box);
  g_nfc_panel = nullptr;
  tool_nfc_read(g_nfc_box);
}

// No ACTION button wired yet on this build -- retry on a timer too, so the
// screen self-heals the moment the PN532 answers without needing any input.
static void nfc_retry_poll_cb(lv_timer_t *t) {
  if (!nfc_begin()) return;
  Serial.println("[nfc] auto-retry: PN532 present");
  lv_timer_del(t);
  g_tool_timer = nullptr;
  if (!g_nfc_box) return;
  lv_obj_clean(g_nfc_box);
  g_nfc_panel = nullptr;
  tool_nfc_read(g_nfc_box);
}

static void nfc_poll_cb(lv_timer_t *t) {
  uint8_t uid[7], len = 0;
  if (nfc_read_uid(uid, &len)) {
    lv_timer_del(g_tool_timer);
    g_tool_timer = nullptr;
    lv_obj_clean(g_nfc_panel);
    nfc_uid_hex(uid, len, g_nfc_uid, sizeof g_nfc_uid);
    make_label(g_nfc_panel, "CARD", &lv_font_unscii_8, C_ACCENT);
    make_label(g_nfc_panel, g_nfc_uid, &lv_font_montserrat_20, C_TXT);
    make_label(g_nfc_panel, len == 4 ? "Mifare Classic / NTAG" : "7-byte UID card",
               &lv_font_montserrat_14, C_SUB);
    g_nfc_status = make_label(g_nfc_panel, "ACTION = save to /nfc", &lv_font_montserrat_14, C_ACCENT_SFT);
    make_label(g_nfc_box, "ACTION saves UID    click = crack keys", &lv_font_unscii_8, C_MUTE);
    g_action_cb = nfc_save_action;
    mascot_play(lv_scr_act(), MASCOT_MISA, LV_ALIGN_BOTTOM_MID, false);
  }
}

static void tool_nfc_read(lv_obj_t *box) {       // RFID/NFC > Read / clone
  lv_obj_t *p = panel(box);
  g_nfc_panel = p;
  g_nfc_box = box;
  if (!nfc_present()) {
    make_label(p, "NFC READ", &lv_font_unscii_8, C_AMBER);
    make_label(p, "PN532 not detected", &lv_font_montserrat_16, C_RED);
    make_label(p, "Check I2C wiring", &lv_font_montserrat_14, C_SUB);
    make_label(box, "retrying every 1.5s - ACTION = retry now", &lv_font_unscii_8, C_MUTE);
    g_action_cb = nfc_retry_action;
    g_tool_timer = lv_timer_create(nfc_retry_poll_cb, 1500, NULL);
    return;
  }
  make_label(p, "PN532 READY", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "tap a card to the antenna", &lv_font_montserrat_16, C_TXT);
  mascot_play(p, MASCOT_NFC_CARD, LV_ALIGN_CENTER, true, 350);
  g_tool_timer = lv_timer_create(nfc_poll_cb, 200, NULL);
}

static lv_obj_t *g_wifi_status = nullptr;         // WiFi scan -> CSV save status
static lv_obj_t *g_wifi_panel = nullptr;
static lv_obj_t *g_wifi_box = nullptr;

static void wifi_save_action() {
  char body[512];
  int o = 0, n = wifi_count();
  for (int i = 0; i < n && o < (int)sizeof body - 80; i++)
    o += snprintf(body + o, sizeof body - o, "%s,%s,%d,%d\n", wifi_ssid(i),
                  wifi_enc_str(wifi_enc(i)), wifi_chan(i), wifi_rssi(i));
  const char *path = storage_save(SAVE_WIFI, "csv", (const uint8_t *)body, (size_t)o);
  if (g_wifi_status)
    lv_label_set_text(g_wifi_status, (path && path[0]) ? path : "no SD card");
}

static void wifi_poll_cb(lv_timer_t *t) {
  int n = wifi_scan_complete();
  if (n == -1) return; // Still scanning
  
  lv_timer_del(g_tool_timer);
  g_tool_timer = nullptr;
  lv_obj_clean(g_wifi_panel);
  
  if (n <= 0) {
    make_label(g_wifi_panel, "WIFI SCAN", &lv_font_unscii_8, C_ACCENT);
    make_label(g_wifi_panel, "no networks found", &lv_font_montserrat_16, C_SUB);
    return;
  }
  char h[24]; snprintf(h, sizeof h, "%d networks", n);
  make_label(g_wifi_panel, h, &lv_font_unscii_8, C_ACCENT);
  int show = n < 5 ? n : 5;
  for (int i = 0; i < show; i++) {
    int enc = wifi_enc(i);
    char line[72];
    snprintf(line, sizeof line, "%s  %s  %d dBm", wifi_ssid(i), wifi_enc_str(enc), wifi_rssi(i));
    make_label(g_wifi_panel, line, &lv_font_montserrat_14, wifi_is_open(enc) ? C_RED : C_TXT);
  }
  g_wifi_status = make_label(g_wifi_panel, "ACTION = save list to /wifi", &lv_font_unscii_8, C_ACCENT_SFT);
  make_label(g_wifi_box, "ACTION saves CSV    click = rescan", &lv_font_unscii_8, C_MUTE);
  g_action_cb = wifi_save_action;
}

static void tool_wifi_scan(lv_obj_t *box) {      // WiFi > Scan / recon
  lv_obj_t *p = panel(box);
  g_wifi_panel = p;
  g_wifi_box = box;
  
  make_label(p, "WIFI SCAN", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "scanning 2.4GHz...", &lv_font_montserrat_16, C_TXT);
  ui_anim_radar_create(p, 60, lv_color_hex(C_CYAN));
  mascot_play(p, MASCOT_L, LV_ALIGN_TOP_RIGHT, true);

  wifi_scan_async();
  g_tool_timer = lv_timer_create(wifi_poll_cb, 200, NULL);
}

static void tool_csi(lv_obj_t *box) {            // See invisible > See through wall
  lv_obj_t *p = panel(box);
  // Demo CSI window with someone moving -> high variance -> motion.
  float win[8] = {40.f, 60.f, 42.f, 58.f, 39.f, 61.f, 41.f, 59.f};
  float var = csi_variance(win, 8);
  bool motion = csi_motion(win, 8, 5.0f);
  make_label(p, "SEE THROUGH WALL", &lv_font_unscii_8, C_CYAN);
  make_label(p, motion ? "MOTION DETECTED" : "room is still",
             &lv_font_montserrat_20, motion ? C_RED : C_ACCENT_SFT);
  char h[40]; snprintf(h, sizeof h, "CSI variance %.0f", var);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "ambient WiFi channel-state sensing", &lv_font_montserrat_14, C_SUB);
  make_label(box, "esp_wifi CSI capture = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_df(lv_obj_t *box) {             // See invisible > Direction finder
  static int prev = -80;
  lv_obj_t *p = panel(box);
  make_label(p, "DIRECTION FINDER", &lv_font_unscii_8, C_ACCENT);
  if (!cc1101_present()) {
    make_label(p, "demo - CC1101 not detected", &lv_font_montserrat_16, C_AMBER);
    make_label(p, "433.92 MHz  -70 dBm  WARMER", &lv_font_montserrat_14, C_SUB);
    return;
  }
  int rssi = cc1101_rssi_at(433.92f);
  int t = df_trend(rssi, prev);
  prev = rssi;
  char h[40]; snprintf(h, sizeof h, "%d dBm  %s", rssi, df_label(t));
  make_label(p, h, &lv_font_montserrat_20, t > 0 ? C_ACCENT : t < 0 ? C_RED : C_TXT);
  make_label(p, "walk around - click to sample", &lv_font_montserrat_14, C_SUB);
  make_label(box, "warmer = closer to the transmitter", &lv_font_unscii_8, C_MUTE);
}

static int       g_nrf_ch = 6;                   // live 2.4 GHz channel inspector
static lv_obj_t *g_nrf_lbl = nullptr;

static void nrf_paint(int ch) {
  if (!g_nrf_lbl) return;
  bool busiest = nrf_present() && nrf_scanned() && ch == nrf_busiest_ch();
  lv_label_set_text_fmt(g_nrf_lbl, "ch %d  -  %d MHz%s", ch, nrf_ch_mhz(ch),
                        busiest ? "   <busiest>" : "");
}

static void tool_nrf_scan(lv_obj_t *box) {       // NRF24 / 2.4GHz > Band scanner
  lv_obj_t *p = panel(box);
  bool live = nrf_present();
  if (live && !nrf_scanned()) nrf_scan();
  make_label(p, "2.4GHz SCAN", &lv_font_unscii_8, live ? C_ACCENT : C_AMBER);
  g_nrf_lbl = make_label(p, "", &lv_font_montserrat_16, C_TXT);
  make_label(p, live ? "rotate to inspect a channel" : "demo - NRF24 not detected",
             &lv_font_montserrat_14, live ? C_SUB : C_AMBER);
  make_label(box, "click = rescan", &lv_font_unscii_8, C_MUTE);
  nrf_paint(g_nrf_ch);
  // Live-select: rotary walks the 126 nRF channels, flags the busiest.
  g_edit_val = &g_nrf_ch;
  g_edit_min = 0; g_edit_max = 125; g_edit_step = 1;
  g_edit_cb = nrf_paint;
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
  make_label(p, h, &lv_font_montserrat_20, C_ACCENT_SFT);
  make_label(p, "unencrypted keyboards only", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "nRF24 ESB sniff = bring-up - own gear only",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_mousejack(lv_obj_t *box) {      // NRF24 / 2.4GHz > Mousejack
  lv_obj_t *p = panel(box);
  const char *payload = "powershell -w hidden";  // demo attack payload
  uint8_t frames[48][UNIFYING_KBD_LEN];
  int n = mousejack_stream(0x00, payload, frames, 48);
  make_label(p, "MOUSEJACK", &lv_font_unscii_8, C_AMBER);
  make_label(p, "inject keystrokes into a dongle", &lv_font_montserrat_16, C_TXT);
  char hx[48]; snprintf(hx, sizeof hx, "payload -> %d HID frames", n);
  make_label(p, hx, &lv_font_unscii_8, C_SUB);
  make_label(p, "unencrypted Logitech Unifying", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "sniff addr + inject = bring-up - own gear only",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_espnow(lv_obj_t *box) {         // Comms / Off-grid > ESP-NOW mesh
  if (!espnow_active()) espnow_begin();
  lv_obj_t *p = panel(box);
  make_label(p, "ESP-NOW MESH", &lv_font_unscii_8, C_ACCENT);
  char h[40]; snprintf(h, sizeof h, "%lu messages received", (unsigned long)espnow_rx());
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  const char *last = espnow_last();
  make_label(p, last[0] ? last : "(no messages yet)", &lv_font_montserrat_14, C_SUB);
  make_label(box, "router-free - click to broadcast ping", &lv_font_unscii_8, C_MUTE);
}

static void tool_usbhost(lv_obj_t *box) {        // Comms / Off-grid > USB host
  lv_obj_t *p = panel(box);
  // Demo MBR: one FAT32 partition, 2048 LBA start.
  uint8_t mbr[512] = {0};
  mbr[510] = 0x55; mbr[511] = 0xAA;
  uint8_t *e = mbr + MBR_PART0_OFF;
  e[4] = 0x0C; e[9] = 0x08; e[12] = 0x40; e[13] = 0x42; e[14] = 0x0F;
  make_label(p, "USB HOST", &lv_font_unscii_8, C_ACCENT);
  make_label(p, mbr_valid(mbr) ? "drive mounted" : "no MBR",
             &lv_font_montserrat_16, C_TXT);
  char h[48]; snprintf(h, sizeof h, "P1 %s  %lu MB", mbr_part_type(e[4]),
                       (unsigned long)(mbr_part_sectors(e) / 2048));
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "browse & copy files to SD", &lv_font_montserrat_14, C_ACCENT_SFT);
  make_label(box, "USB MSC host enumeration = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_usage(lv_obj_t *box) {          // Me > Claude usage
  lv_obj_t *p = panel(box);
  int pct = usage_pct(62, 100);                  // demo — real data via phone bridge
  make_label(p, "CLAUDE USAGE", &lv_font_unscii_8, C_ACCENT);
  char h[16]; snprintf(h, sizeof h, "%d%% of 5h", pct);
  make_label(p, h, &lv_font_montserrat_28, pct > 85 ? C_RED : C_ACCENT);
  lv_obj_t *bar = lv_bar_create(p);
  lv_obj_set_size(bar, lv_pct(100), 10);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
  lv_bar_set_value(bar, pct, LV_ANIM_OFF);
  char r[24]; fmt_hms(12180, r, sizeof r);
  char line[40]; snprintf(line, sizeof line, "resets in %s", r);
  make_label(p, line, &lv_font_montserrat_14, C_SUB);
  make_label(box, "demo - real data via phone bridge", &lv_font_unscii_8, C_MUTE);
}

static void tool_tasks(lv_obj_t *box) {          // Me > Tasks
  lv_obj_t *p = panel(box);
  make_label(p, "TASKS", &lv_font_unscii_8, C_ACCENT);
  // Phone bridge writes /me/tasks.txt to SD; fall back to a demo list if absent.
  static char buf[1024];
  static const char *demo =
    "[ ] !1 Solder the BL mod\n[x] Flash firmware m2\n[ ] !3 Order antennas\n";
  bool sd = storage_read_file("/me/tasks.txt", buf, sizeof buf) > 0;
  const char *text = sd ? buf : demo;
  int shown = 0;
  for (const char *ln = text; ln && *ln && shown < 6; ) {
    char line[64]; size_t n = 0;
    while (ln[n] && ln[n] != '\n' && ln[n] != '\r' && n + 1 < sizeof line) {
      line[n] = ln[n]; n++;
    }
    line[n] = 0;
    bool done; int prio; const char *tx;
    if (task_parse(line, &done, &prio, &tx)) {
      char row[48];
      snprintf(row, sizeof row, "%s %s%s", done ? "[x]" : "[ ]",
               prio == 1 ? "! " : "  ", tx);
      make_label(p, row, &lv_font_unscii_8, done ? C_MUTE : C_TXT);
      shown++;
    }
    const char *nl = ln; while (*nl && *nl != '\n') nl++;
    ln = (*nl == '\n') ? nl + 1 : 0;
  }
  make_label(box, sd ? "loaded /me/tasks.txt  (phone sync = bring-up)"
                     : "demo - phone writes /me/tasks.txt (bring-up)",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_calendar(lv_obj_t *box) {       // Me > Calendar
  lv_obj_t *p = panel(box);
  make_label(p, "CALENDAR", &lv_font_unscii_8, C_ACCENT);
  // Phone bridge writes /me/calendar.txt ("<iCal-dt> <title>" per line) to SD.
  static char buf[512];
  char when[32], title[48];
  bool sd = storage_read_file("/me/calendar.txt", buf, sizeof buf) > 0 &&
            ical_next_event(buf, when, sizeof when, title, sizeof title);
  if (!sd) {                                     // demo fallback
    ical_friendly("20260719T143000Z", when, sizeof when);
    snprintf(title, sizeof title, "Team sync");
  }
  make_label(p, title, &lv_font_montserrat_20, C_TXT);
  make_label(p, when, &lv_font_montserrat_16, C_ACCENT_SFT);
  make_label(p, "next event", &lv_font_montserrat_14, C_SUB);
  make_label(box, sd ? "loaded /me/calendar.txt  (BLE sync = bring-up)"
                     : "demo - phone writes /me/calendar.txt (bring-up)",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_badusb(lv_obj_t *box) {         // BadUSB / HID > DuckyScript
  // Info only — never auto-runs a payload on screen build (that would type into
  // whatever's plugged in). Running is a deliberate ACTION-key step (next iter).
  lv_obj_t *p = panel(box);
  make_label(p, "BADUSB / HID", &lv_font_unscii_8, C_RED);
  make_label(p, "acts as a USB keyboard", &lv_font_montserrat_16, C_TXT);
  make_label(p, "payload: STRING / GUI r / DELAY / ENTER", &lv_font_montserrat_14, C_SUB);
  make_label(p, "load a .txt from SD, then run", &lv_font_montserrat_14, C_ACCENT_SFT);
  make_label(box, "only on machines you own", &lv_font_unscii_8, C_MUTE);
}

static void tool_hidattack(lv_obj_t *box) {      // BadUSB / HID > HID attacks
  // Info only — never types on screen build; firing is a deliberate ACTION step.
  lv_obj_t *p = panel(box);
  const char *demo = "whoami";                   // encodes to HID reports on run
  bool sh; int ok = 0;
  for (const char *c = demo; *c; c++) if (ascii_to_hid(*c, &sh)) ok++;
  make_label(p, "HID ATTACKS", &lv_font_unscii_8, C_RED);
  make_label(p, "type any string as a keyboard", &lv_font_montserrat_16, C_TXT);
  char h[40]; snprintf(h, sizeof h, "\"%s\" -> %d HID keys", demo, ok);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "full ASCII incl. shifted symbols", &lv_font_montserrat_14, C_ACCENT_SFT);
  make_label(box, "only on machines you own - USB HID bring-up",
             &lv_font_unscii_8, C_MUTE);
}

static lv_obj_t *g_tvb_status = nullptr;

static void tvb_fire_action() {
  tvbgone_fire_all(150);
  if (g_tvb_status) lv_label_set_text(g_tvb_status, "blasted all codes");
}

static void tool_tvbgone(lv_obj_t *box) {        // Pranks / IR > TV-B-Gone
  // Info only — never blasts on screen build; firing is a deliberate ACTION step.
  lv_obj_t *p = panel(box);
  make_label(p, "TV-B-GONE", &lv_font_unscii_8, C_ACCENT);
  char h[40]; snprintf(h, sizeof h, "%d TV power codes ready", tvb_count());
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  make_label(p, "Samsung / LG / Sony / NEC / Philips", &lv_font_montserrat_14, C_SUB);
  g_tvb_status = make_label(box, "ACTION = blast all (turns TVs off)", &lv_font_unscii_8, C_MUTE);
  g_action_cb = tvb_fire_action;
}

// Combined TV + A/C brand browser. Index 0..TV-1 = TV brands (irdb.h), the rest
// = A/C brands (ac_db.h). Repaints name + code/protocol in place.
static void ir_brand_edit_cb(int idx) {
  int tvN = ir_brand_count();
  if (idx < tvN) {
    const IrBrand *b = ir_brand_at(idx);
    if (g_ir_name_lbl) lv_label_set_text_fmt(g_ir_name_lbl, "TV   %s", b->name);
    if (g_ir_code_lbl)
      lv_label_set_text_fmt(g_ir_code_lbl, "POWER  0x%08lX", (unsigned long)b->power);
  } else {
    const AcBrand *a = ac_brand_at(idx - tvN);
    if (!a) return;
    if (g_ir_name_lbl) lv_label_set_text_fmt(g_ir_name_lbl, "A/C  %s", a->name);
    if (g_ir_code_lbl)
      lv_label_set_text_fmt(g_ir_code_lbl, "%s %d\xC2\xB0""C   IRac #%d",
                            ac_mode_name(AC_COOL), ac_clamp_temp(g_ac_temp), a->proto);
  }
}

// ACTION on the universal remote bumps the A/C setpoint (16..30, wrap) and repaints.
static void ir_ac_temp_bump() {
  g_ac_temp = g_ac_temp >= 30 ? 16 : g_ac_temp + 1;
  ir_brand_edit_cb(g_ir_brand);
}

static void tool_ir_universal(lv_obj_t *box) {   // IR > Universal remote
  lv_obj_t *p = panel(box);
  int total = ir_brand_count() + AC_BRAND_COUNT;
  if (g_ir_brand < 0 || g_ir_brand >= total) g_ir_brand = 0;
  make_label(p, "UNIVERSAL REMOTE", &lv_font_unscii_8, C_ACCENT);
  g_ir_name_lbl = make_label(p, "", &lv_font_montserrat_20, C_TXT);
  g_ir_code_lbl = make_label(p, "", &lv_font_unscii_8, C_SUB);
  char n[40]; snprintf(n, sizeof n, "%d TV + %d A/C brands", ir_brand_count(), AC_BRAND_COUNT);
  make_label(p, n, &lv_font_montserrat_14, C_ACCENT_SFT);
  if (storage_ready()) {                          // plus any Flipper IRDB on SD
    char s[40]; snprintf(s, sizeof s, "+ SD IRDB: %d .ir files", storage_count_files("/ir", ".ir"));
    make_label(p, s, &lv_font_unscii_8, C_SUB);
  }
  make_label(box, "rotate = brand   ACTION = A/C temp   click = blast",
             &lv_font_unscii_8, C_MUTE);
  ir_brand_edit_cb(g_ir_brand);                  // paint the current selection
  // Live-select across the whole TV+AC catalogue; ACTION adjusts A/C temp.
  g_edit_val = &g_ir_brand;
  g_edit_min = 0; g_edit_max = total - 1; g_edit_step = 1;
  g_edit_cb = ir_brand_edit_cb;
  g_action_cb = ir_ac_temp_bump;
}

// Last learned IR frame + save status; ACTION writes it to /ir.
static uint8_t  g_irl_proto = 3;                 // NEC
static uint64_t g_irl_value = 0x00000408;        // demo addr/cmd
static uint16_t g_irl_bits = 32;
static lv_obj_t *g_irl_status = nullptr;
static lv_obj_t *g_irl_panel = nullptr;

static void ir_save_action() {
  char body[80];
  int n = snprintf(body, sizeof body, "protocol:%u\nbits:%u\nvalue:0x%08llX\n",
                   (unsigned)g_irl_proto, (unsigned)g_irl_bits,
                   (unsigned long long)g_irl_value);
  const char *path = storage_save(SAVE_IR, "ir", (const uint8_t *)body, (size_t)n);
  if (g_irl_status)
    lv_label_set_text(g_irl_status, (path && path[0]) ? path : "no SD card");
}

static void ir_poll_cb(lv_timer_t *t) {
  uint8_t proto; uint64_t value; uint16_t bits;
  if (ir_learn(10, &proto, &value, &bits)) {
    Serial.printf("[ir] captured proto=%u bits=%u value=0x%08llX\n",
                  (unsigned)proto, (unsigned)bits, (unsigned long long)value);
    lv_timer_del(g_tool_timer);
    g_tool_timer = nullptr;
    lv_obj_clean(g_irl_panel);
    g_irl_proto = proto; g_irl_value = value; g_irl_bits = bits;
    make_label(g_irl_panel, "CAPTURED", &lv_font_unscii_8, C_ACCENT);
    char h[40]; snprintf(h, sizeof h, "0x%08llX", (unsigned long long)value);
    make_label(g_irl_panel, h, &lv_font_montserrat_20, C_TXT);
    char sub[40]; snprintf(sub, sizeof sub, "proto %u  %u bits", (unsigned)proto, (unsigned)bits);
    make_label(g_irl_panel, sub, &lv_font_montserrat_14, C_ACCENT_SFT);
    g_irl_status = make_label(g_irl_panel, "ACTION = save to /ir", &lv_font_unscii_8, C_ACCENT_SFT);
    g_action_cb = ir_save_action;
    mascot_play(lv_scr_act(), MASCOT_MISA, LV_ALIGN_BOTTOM_MID, false);
  }
}

static void tool_ir_learn(lv_obj_t *box) {       // IR > Learn & blast
  lv_obj_t *p = panel(box);
  g_irl_panel = p;
  make_label(p, "IR LEARN / BLAST", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "TX GPIO47   RX GPIO48", &lv_font_unscii_8, C_SUB);
  make_label(p, "aim any remote and press a button", &lv_font_montserrat_16, C_TXT);
  mascot_play(p, MASCOT_IR_BEAM, LV_ALIGN_CENTER, true, 300);
  make_label(box, "ACTION saves .ir    click = blast", &lv_font_unscii_8, C_MUTE);
  g_action_cb = ir_save_action;
  g_tool_timer = lv_timer_create(ir_poll_cb, 100, NULL);
}

static lv_obj_t *g_ble_panel = nullptr;
static lv_obj_t *g_ble_box = nullptr;

static void ble_poll_cb(lv_timer_t *t) {
  if (!ble_scan_complete()) return;
  
  lv_timer_del(g_tool_timer);
  g_tool_timer = nullptr;
  lv_obj_clean(g_ble_panel);
  
  int n = ble_count();
  if (n <= 0) {
    make_label(g_ble_panel, "BLE SCAN", &lv_font_unscii_8, C_ACCENT);
    make_label(g_ble_panel, "nothing advertising nearby", &lv_font_montserrat_16, C_SUB);
    return;
  }
  char h[24]; snprintf(h, sizeof h, "%d devices", n);
  make_label(g_ble_panel, h, &lv_font_unscii_8, C_ACCENT);
  int show = n < 5 ? n : 5;
  for (int i = 0; i < show; i++) {
    const char *nm = ble_name(i);
    if (!nm[0]) nm = ble_addr(i);
    char line[72];
    snprintf(line, sizeof line, "%s  %d dBm%s", nm, ble_rssi(i),
             ble_is_tracker(i) ? "  [TRACKER]" : "");
    make_label(g_ble_panel, line, &lv_font_montserrat_14, ble_is_tracker(i) ? C_CYAN : C_TXT);
  }
  make_label(g_ble_box, "click = rescan    cyan = tracker", &lv_font_unscii_8, C_MUTE);
}

static void tool_ble_scan(lv_obj_t *box) {       // Bluetooth > Scan / recon
  lv_obj_t *p = panel(box);
  g_ble_panel = p;
  g_ble_box = box;
  
  make_label(p, "BLE SCAN", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "scanning 3 seconds...", &lv_font_montserrat_16, C_TXT);
  ui_anim_radar_create(p, 60, lv_color_hex(C_CYAN));
  mascot_play(p, MASCOT_L, LV_ALIGN_TOP_RIGHT, true);

  ble_scan_async(3);
  g_tool_timer = lv_timer_create(ble_poll_cb, 200, NULL);
}

static lv_obj_t *g_camera_panel = nullptr;

static void camera_poll_cb(lv_timer_t *t) {
  int n = wifi_scan_complete();
  if (n == -1) return;
  
  lv_timer_del(g_tool_timer);
  g_tool_timer = nullptr;
  lv_obj_clean(g_camera_panel);
  
  make_label(g_camera_panel, "HIDDEN CAMERA", &lv_font_unscii_8, C_CYAN);
  int cams = 0;
  for (int i = 0; i < n; i++) {
    const char *b = camera_ssid_brand(wifi_ssid(i));
    if (!b) continue;
    cams++;
    char line[64]; snprintf(line, sizeof line, "%s  (%s)", wifi_ssid(i), b);
    make_label(g_camera_panel, line, &lv_font_montserrat_14, C_AMBER);
  }
  if (cams == 0)
    make_label(g_camera_panel, n > 0 ? "no camera-like WiFi APs" : "scan failed",
               &lv_font_montserrat_16, C_ACCENT_SFT);
}

static void tool_camera(lv_obj_t *box) {         // Am I safe? > Hidden camera
  lv_obj_t *p = panel(box);
  g_camera_panel = p;
  
  make_label(p, "HIDDEN CAMERA", &lv_font_unscii_8, C_CYAN);
  make_label(p, "scanning 2.4GHz...", &lv_font_montserrat_16, C_TXT);
  ui_anim_radar_create(p, 60, lv_color_hex(C_CYAN));
  
  wifi_scan_async();
  g_tool_timer = lv_timer_create(camera_poll_cb, 200, NULL);
  
  make_label(box, "WiFi-name heuristic - OUI check next", &lv_font_unscii_8, C_MUTE);
}

static void tool_deauth(lv_obj_t *box) {         // Am I safe? > Deauth detector
  if (!deauth_active()) deauth_begin();
  lv_obj_t *p = panel(box);
  make_label(p, "DEAUTH DETECTOR", &lv_font_unscii_8, C_CYAN);
  uint32_t hits = deauth_count();
  char h[40]; snprintf(h, sizeof h, "%lu deauth frames seen", (unsigned long)hits);
  make_label(p, h, &lv_font_montserrat_16, hits > 0 ? C_RED : C_ACCENT_SFT);
  make_label(p, hits > 0 ? "someone may be jamming WiFi near you" : "airwaves look clean",
             &lv_font_montserrat_14, C_SUB);
  make_label(box, "watching 802.11 management frames", &lv_font_unscii_8, C_MUTE);
}

// Repaint the selected gag's name + the exact DuckyScript line (encoder edit cb).
static void gag_paint(int idx) {
  if (idx < 0 || idx >= GAG_COUNT) return;
  if (g_gag_name) lv_label_set_text(g_gag_name, GAGS[idx].name);
  if (g_gag_line) lv_label_set_text(g_gag_line, GAGS[idx].line);
  if (g_gag_status) lv_label_set_text(g_gag_status, "ACTION = type it (USB HID bring-up)");
}

// ACTION press only: type the selected gag into the plugged-in host. The actual
// USBHIDKeyboard playback is on-device bring-up (needs a real USB host attached).
static void gag_run_action() {
  if (g_gag < 0 || g_gag >= GAG_COUNT) return;
  badusb_begin();
  badusb_run_line(GAGS[g_gag].line);
  if (g_gag_status)
    lv_label_set_text(g_gag_status, badusb_ready() ? "sent" : "no USB host");
}

static void tool_usbgag(lv_obj_t *box) {         // Pranks > USB gag
  // Never types on screen build; running is a deliberate ACTION step.
  if (g_gag < 0 || g_gag >= GAG_COUNT) g_gag = 0;
  lv_obj_t *p = panel(box);
  make_label(p, "USB GAG", &lv_font_unscii_8, C_RED);
  g_gag_name = make_label(p, "", &lv_font_montserrat_20, C_TXT);
  g_gag_line = make_label(p, "", &lv_font_unscii_8, C_ACCENT_SFT);
  make_label(p, "harmless: opens a page / locks screen", &lv_font_montserrat_14, C_SUB);
  g_gag_status = make_label(box, "", &lv_font_unscii_8, C_MUTE);
  gag_paint(g_gag);
  // Live-select: encoder cycles the gag; ACTION types it (own machines only).
  g_edit_val = &g_gag;
  g_edit_min = 0; g_edit_max = GAG_COUNT - 1; g_edit_step = 1;
  g_edit_cb = gag_paint;
  g_action_cb = gag_run_action;
}

static void tool_rickroll(lv_obj_t *box) {       // Pranks > Rickroll tag
  lv_obj_t *p = panel(box);
  const char *url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
  uint8_t rec[80], tag[96];
  size_t rn = ndef_uri_record(url, rec, sizeof rec);
  size_t tn = ndef_tlv_wrap(rec, rn, tag, sizeof tag);
  make_label(p, "RICKROLL TAG", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "write an NFC tag -> opens the song", &lv_font_montserrat_16, C_TXT);
  char h[40]; snprintf(h, sizeof h, "%u-byte NDEF tag ready", (unsigned)tn);
  make_label(p, h, &lv_font_unscii_8, C_SUB);
  make_label(p, "tap a phone to it, watch the face", &lv_font_montserrat_14, C_ACCENT_SFT);
  make_label(box, "PN532 tag write = bring-up", &lv_font_unscii_8, C_MUTE);
}

static void tool_hackscreen(lv_obj_t *box) {     // Pranks > Hacker screen
  lv_obj_t *p = panel(box);
  make_label(p, "ACCESS GRANTED", &lv_font_unscii_8, C_ACCENT);
  uint32_t s = 0xC0FFEE;                          // static frames; animate = enhancement
  for (int i = 0; i < 3; i++) {
    char line[25]; hack_line(&s, line, 24);
    make_label(p, line, &lv_font_unscii_8, C_ACCENT_SFT);
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
  make_label(p, "CAST CRASHER", &lv_font_unscii_8, C_ACCENT);
  make_label(p, cast_kind(st), &lv_font_montserrat_20, C_TXT);
  make_label(p, "192.168.1.42:8008", &lv_font_unscii_8, C_SUB);
  make_label(p, "queue a video on nearby TVs (fun)",
             &lv_font_montserrat_14, C_ACCENT_SFT);
  make_label(box, "SSDP discovery + DIAL = bring-up", &lv_font_unscii_8, C_MUTE);
}

#include "esp_flash.h"

static int s_bug_idx = 0;
static const float s_bug_freqs[] = { 88.0, 92.5, 96.5, 102.1, 107.9, 144.0, 155.0, 168.0, 433.92, 868.0, 915.0 };
static int s_bug_rssi[11];
static lv_timer_t *s_active_tool_timer = nullptr;

static void tool_timer_cleanup() {
    if (s_active_tool_timer) {
        lv_timer_del(s_active_tool_timer);
        s_active_tool_timer = nullptr;
    }
}

static void audiobug_timer_cb(lv_timer_t * t) {
  lv_obj_t *box = (lv_obj_t *)t->user_data;
  
  if (s_bug_idx < 11) {
     s_bug_rssi[s_bug_idx] = cc1101_rssi_at(s_bug_freqs[s_bug_idx]);
     s_bug_idx++;
     return;
  }
  
  lv_timer_pause(t);
  int max_idx = 0;
  for (int i = 1; i < 11; i++) {
     if (s_bug_rssi[i] > s_bug_rssi[max_idx]) max_idx = i;
  }
  float peak = s_bug_freqs[max_idx];
  int peak_val = s_bug_rssi[max_idx];
  const char *band = bug_band(peak);
  bool active = (peak_val > -75);
  
  lv_obj_clean(box);
  lv_obj_t *p = panel(box);
  make_label(p, "BUG SWEEP", &lv_font_unscii_8, C_CYAN);
  if (active) {
      char h[40]; snprintf(h, sizeof h, "peak %.1f MHz (%ddBm)", peak, peak_val);
      make_label(p, h, &lv_font_montserrat_20, C_TXT);
      make_label(p, band ? band : "UNKNOWN BAND", &lv_font_montserrat_16, C_RED);
  } else {
      make_label(p, "no covert bands active", &lv_font_montserrat_16, C_ACCENT_SFT);
  }
  make_label(p, "FM / VHF / UHF / GSM / 2.4G", &lv_font_unscii_8, C_SUB);
  make_label(box, "Press action to sweep again", &lv_font_unscii_8, C_MUTE);
}

static void tool_audiobug(lv_obj_t *box) {
  lv_obj_t *p = panel(box);
  make_label(p, "BUG SWEEP", &lv_font_unscii_8, C_CYAN);
  
  if (!cc1101_present()) {
      make_label(p, "CC1101 not detected", &lv_font_montserrat_16, C_RED);
      return;
  }
  make_label(p, "Sweeping bands...", &lv_font_montserrat_16, C_TXT);
  ui_anim_waves_create(p, 80, lv_color_hex(C_RED));
  
  s_bug_idx = 0;
  s_active_tool_timer = lv_timer_create(audiobug_timer_cb, 50, box);
  g_cleanup_cb = tool_timer_cleanup;
}

static void skimmer_timer_cb(lv_timer_t * t) {
  lv_obj_t *box = (lv_obj_t *)t->user_data;
  if (!ble_scan_complete()) return;
  lv_timer_pause(t);
  
  int count = ble_count();
  const char* suspect = NULL;
  for (int i=0; i<count; i++) {
     if (is_skimmer_name(ble_name(i))) {
         suspect = ble_name(i);
         break;
     }
  }
  
  lv_obj_clean(box);
  lv_obj_t *p = panel(box);
  make_label(p, "SKIMMER DETECTOR", &lv_font_unscii_8, C_CYAN);
  if (suspect) {
      make_label(p, "SUSPECT MODULE NEARBY", &lv_font_montserrat_16, C_RED);
      char h[40]; snprintf(h, sizeof h, "BLE name: \"%s\"", suspect);
      make_label(p, h, &lv_font_unscii_8, C_SUB);
  } else {
      make_label(p, "no skimmer signatures", &lv_font_montserrat_16, C_ACCENT_SFT);
      char h[40]; snprintf(h, sizeof h, "Scanned %d devices", count);
      make_label(p, h, &lv_font_unscii_8, C_SUB);
  }
  make_label(p, "generic BT modules used by skimmers", &lv_font_montserrat_14, C_SUB);
  make_label(box, "Press action to scan again", &lv_font_unscii_8, C_MUTE);
}

static void tool_skimmer(lv_obj_t *box) {
  lv_obj_t *p = panel(box);
  make_label(p, "SKIMMER DETECTOR", &lv_font_unscii_8, C_CYAN);
  make_label(p, "Scanning BLE...", &lv_font_montserrat_16, C_TXT);
  ui_anim_radar_create(p, 80, lv_color_hex(C_ACCENT));
  
  ble_scan_async(3);
  s_active_tool_timer = lv_timer_create(skimmer_timer_cb, 500, box);
  g_cleanup_cb = tool_timer_cleanup;
}

static void droneid_timer_cb(lv_timer_t * t) {
  lv_obj_t *box = (lv_obj_t *)t->user_data;
  if (!ble_scan_complete()) return;
  lv_timer_pause(t);
  
  int count = ble_count();
  const char* suspect = NULL;
  for (int i=0; i<count; i++) {
     const char* n = ble_name(i);
     if (ci_contains(n, "DJI") || ci_contains(n, "Drone") || ci_contains(n, "Mavic")) {
         suspect = n;
         break;
     }
  }
  
  lv_obj_clean(box);
  lv_obj_t *p = panel(box);
  make_label(p, "DRONE SPOTTER", &lv_font_unscii_8, C_CYAN);
  if (suspect) {
      make_label(p, "Drone broadcast nearby", &lv_font_montserrat_16, C_TXT);
      char h[48]; snprintf(h, sizeof h, "ID %s", suspect);
      make_label(p, h, &lv_font_unscii_8, C_SUB);
  } else {
      make_label(p, "No drones detected", &lv_font_montserrat_16, C_ACCENT_SFT);
  }
  make_label(box, "Press action to sniff again", &lv_font_unscii_8, C_MUTE);
}

static void tool_droneid(lv_obj_t *box) {
  lv_obj_t *p = panel(box);
  make_label(p, "DRONE SPOTTER", &lv_font_unscii_8, C_CYAN);
  make_label(p, "Sniffing airspace...", &lv_font_montserrat_16, C_TXT);
  ui_anim_radar_create(p, 80, lv_color_hex(C_CYAN));
  
  ble_scan_async(5);
  s_active_tool_timer = lv_timer_create(droneid_timer_cb, 500, box);
  g_cleanup_cb = tool_timer_cleanup;
}

#define FWDUMP_BYTES (64 * 1024)   // bounded chunk from offset 0 -- storage_save is one-shot (new file/call)
static lv_obj_t *g_fwdump_status = nullptr;

static void fwdump_action() {
  if (!g_fwdump_status) return;
  uint8_t *buf = (uint8_t *)ps_malloc(FWDUMP_BYTES);
  if (!buf) { lv_label_set_text(g_fwdump_status, "PSRAM alloc failed"); return; }
  bool ok = esp_flash_read(NULL, buf, 0, FWDUMP_BYTES) == ESP_OK;
  const char *path = ok ? storage_save(SAVE_FW, "bin", buf, FWDUMP_BYTES) : "";
  free(buf);
  if (!ok) lv_label_set_text(g_fwdump_status, "flash read failed");
  else if (!path || !path[0]) lv_label_set_text(g_fwdump_status, "no SD card");
  else { char h[64]; snprintf(h, sizeof h, "dumped 64KB -> %s", path); lv_label_set_text(g_fwdump_status, h); }
}

static void tool_fwdump(lv_obj_t *box) {
  lv_obj_t *p = panel(box);
  uint32_t id = 0;
  esp_flash_read_id(NULL, &id);
  uint8_t jedec[3] = { (uint8_t)(id & 0xFF), (uint8_t)((id >> 8) & 0xFF), (uint8_t)((id >> 16) & 0xFF) };
  uint32_t bytes = jedec_capacity_bytes(jedec[2]);

  make_label(p, "FIRMWARE DUMP", &lv_font_unscii_8, C_ACCENT);
  char id_str[40]; snprintf(id_str, sizeof id_str, "JEDEC %02X %02X %02X", jedec[0], jedec[1], jedec[2]);
  make_label(p, id_str, &lv_font_unscii_8, C_SUB);
  char h[48]; snprintf(h, sizeof h, "%s  -  %lu MB", jedec_manuf(jedec[0]), (unsigned long)(bytes / (1024 * 1024)));
  make_label(p, h, &lv_font_montserrat_16, C_TXT);
  make_label(p, "Internal Flash (ESP32)", &lv_font_montserrat_14, C_ACCENT_SFT);
  g_fwdump_status = make_label(box, "ACTION = dump first 64KB to SD", &lv_font_unscii_8, C_MUTE);
  g_action_cb = fwdump_action;
}

static int       g_gpio_pin = 5;                 // live GPIO selector state
static bool      g_gpio_state = false;            // last-driven level (per visit)
static lv_obj_t *g_gpio_lbl = nullptr;

static void gpio_paint(int pin) {
  if (!g_gpio_lbl) return;
  bool ok = gpio_usable(pin);
  lv_label_set_text_fmt(g_gpio_lbl, "GPIO %d  -  %s", pin,
                        ok ? "safe to drive" : "RESERVED");
  lv_obj_set_style_text_color(g_gpio_lbl, lv_color_hex(ok ? C_TXT : C_RED), 0);
}

static void gpio_toggle_action() {
  if (!gpio_usable(g_gpio_pin)) return;
  g_gpio_state = !g_gpio_state;
  pinMode(g_gpio_pin, OUTPUT);
  digitalWrite(g_gpio_pin, g_gpio_state ? HIGH : LOW);
  if (g_gpio_lbl)
    lv_label_set_text_fmt(g_gpio_lbl, "GPIO %d  -  driven %s", g_gpio_pin,
                          g_gpio_state ? "HIGH" : "LOW");
}

static void tool_gpio(lv_obj_t *box) {           // Tools > GPIO play
  lv_obj_t *p = panel(box);
  make_label(p, "GPIO PLAY", &lv_font_unscii_8, C_ACCENT);
  g_gpio_lbl = make_label(p, "", &lv_font_montserrat_20, C_TXT);
  make_label(p, "toggle HIGH/LOW, read state", &lv_font_montserrat_14, C_SUB);
  make_label(p, "flash/PSRAM pins 26-37 locked out", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "rotate = pin   ACTION = toggle",
             &lv_font_unscii_8, C_MUTE);
  gpio_paint(g_gpio_pin);
  // Live-select: rotary walks GPIO 0..48, colouring reserved pins red.
  g_edit_val = &g_gpio_pin;
  g_edit_min = 0; g_edit_max = 48; g_edit_step = 1;
  g_edit_cb = gpio_paint;
  g_action_cb = gpio_toggle_action;
}

static lv_obj_t *g_i2c_panel = nullptr;

static void i2c_scan_paint() {
  if (!g_i2c_panel) return;
  lv_obj_clean(g_i2c_panel);
  lv_obj_t *p = g_i2c_panel;
  make_label(p, "I2C SCAN", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "live probe, 0x08-0x77", &lv_font_montserrat_16, C_TXT);
  uint8_t found[16]; int nfound = 0;
  for (uint8_t a = 0x08; a <= 0x77 && nfound < 16; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) found[nfound++] = a;
  }
  if (nfound == 0) {
    make_label(p, "no devices answered", &lv_font_montserrat_16, C_RED);
  } else {
    char h[80]; int o = 0;
    for (int k = 0; k < nfound; k++)
      o += snprintf(h + o, sizeof h - o, k ? " %02X" : "%02X", found[k]);
    make_label(p, h, &lv_font_unscii_8, C_SUB);
    for (int k = 0; k < nfound; k++) {
      const char *nm = i2c_device_name(found[k]);
      if (nm) make_label(p, nm, &lv_font_montserrat_14, C_ACCENT_SFT);
    }
  }
}

static void tool_buspirate(lv_obj_t *box) {      // Tools > Bus Pirate
  g_i2c_panel = panel(box);
  i2c_scan_paint();
  make_label(box, "ACTION = rescan", &lv_font_unscii_8, C_MUTE);
  g_action_cb = i2c_scan_paint;
}

static void tool_wardrive(lv_obj_t *box) {       // WiFi > Wardrive
  lv_obj_t *p = panel(box);
  char row[128];
  wardrive_csv("A4:2B:B0:11:22:33", "linksys", "[WPA2-PSK-CCMP][ESS]",
               6, -52, 12.971600, 77.594600, row, sizeof row);
  make_label(p, "WARDRIVE", &lv_font_unscii_8, C_ACCENT);
  make_label(p, "log every AP + GPS -> SD (WiGLE)", &lv_font_montserrat_16, C_TXT);
  make_label(p, "1 net  -  fix 12.9716,77.5946", &lv_font_unscii_8, C_SUB);
  make_label(p, "wardrive.csv ready to upload", &lv_font_montserrat_14, C_ACCENT_SFT);
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
             &lv_font_montserrat_14, crack ? C_ACCENT_SFT : C_AMBER);
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
  make_label(p, "deauth + disassoc, both directions", &lv_font_unscii_8, C_SUB);
  make_label(p, "YOUR OWN NETWORK ONLY", &lv_font_montserrat_14, C_AMBER);
  make_label(box, "pick AP + client, click to send - TX bring-up",
             &lv_font_unscii_8, C_MUTE);
}

static void tool_ibutton(lv_obj_t *box) {        // RFID/NFC > iButton key
  lv_obj_t *p = panel(box);
  make_label(p, "IBUTTON / 1-WIRE", &lv_font_unscii_8, C_ACCENT);
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
  make_label(p, "TRANSIT CARD", &lv_font_unscii_8, C_ACCENT);
  if (!nfc_present()) {
    make_label(p, "PN532 not detected", &lv_font_montserrat_16, C_RED);
    return;
  }
  uint8_t uid[7], len = 0;
  make_label(p, nfc_read_uid(uid, &len) ? "card present - reading balance..."
                                        : "tap a metro card",
             &lv_font_montserrat_16, C_TXT);
  make_label(box, "balance block is card-specific (bring-up)", &lv_font_unscii_8, C_MUTE);
}

static uint8_t   g_ndef_tag[96];
static size_t    g_ndef_tag_len = 0;
static lv_obj_t *g_ndef_status = nullptr;

static void ndef_write_action() {
  if (!g_ndef_status) return;
  if (!nfc_present()) { lv_label_set_text(g_ndef_status, "PN532 not detected"); return; }
  uint8_t uid[7], len = 0;
  if (!nfc_read_uid(uid, &len)) { lv_label_set_text(g_ndef_status, "no tag - tap one first"); return; }
  lv_label_set_text(g_ndef_status,
    nfc_write_ndef(g_ndef_tag, g_ndef_tag_len) ? "written!" : "write failed");
}

static void tool_ndef(lv_obj_t *box) {           // RFID/NFC > Write NDEF tag
  lv_obj_t *p = panel(box);
  const char *url = "https://github.com/kavin-jain";
  uint8_t rec[64];
  size_t n = ndef_uri_record(url, rec, sizeof rec);
  g_ndef_tag_len = ndef_tlv_wrap(rec, n, g_ndef_tag, sizeof g_ndef_tag);
  make_label(p, "WRITE NDEF TAG", &lv_font_unscii_8, C_ACCENT);
  make_label(p, url, &lv_font_montserrat_14, C_TXT);
  char h[40]; snprintf(h, sizeof h, "NDEF record ready: %u bytes", (unsigned)n);
  make_label(p, h, &lv_font_montserrat_14, C_ACCENT_SFT);
  g_ndef_status = make_label(p, nfc_present() ? "tap an NTAG, ACTION to write"
                                              : "PN532 not detected",
             &lv_font_montserrat_14, C_SUB);
  g_action_cb = ndef_write_action;
}

static void tool_gatt(lv_obj_t *box) {           // Bluetooth > GATT explore
  lv_obj_t *p = panel(box);
  make_label(p, "GATT EXPLORE", &lv_font_unscii_8, C_ACCENT);
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
               &lv_font_montserrat_16, C_ACCENT_SFT);
  make_label(box, "spots Flippers / pwnagotchis / Marauders", &lv_font_unscii_8, C_MUTE);
}

static void tool_tracker_hunt(lv_obj_t *box) {   // Bluetooth > Tracker hunt
  lv_obj_t *p = panel(box);
  const uint8_t demo[] = {0x4C, 0x00, 0x12, 0x19};   // Apple AirTag adv data
  const char *brand = ble_tracker_brand(ble_company_id(demo, sizeof demo));
  make_label(p, "TRACKER HUNT", &lv_font_unscii_8, C_CYAN);
  make_label(p, "find AirTag / Tile / SmartTag", &lv_font_montserrat_16, C_TXT);
  char h[40]; snprintf(h, sizeof h, "nearest: %s", brand ? brand : "none");
  make_label(p, h, &lv_font_montserrat_20, C_ACCENT_SFT);
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
               &lv_font_montserrat_16, C_ACCENT_SFT);
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
  make_label(p, "> live data lands next milestone", &lv_font_montserrat_14, C_ACCENT_SFT);
}

static void build_tool(int c, int i) {
  const Tool &t = CATS[c].tools[i];
  lv_obj_t *scr = new_screen(CATS[c].name);
  section(scr, t.code);
  lv_obj_t *box = content_box(scr);
  
  if (t.render) {
    t.render(box);
  } else {
    tool_generic(box, t);
  }
  if (t.kind == K_ATK) {   // brief flash-in: Light for "writing" tools, Ryuk otherwise
    bool writes = strcmp(t.code, "DK") == 0 || strcmp(t.code, "HID") == 0;
    mascot_play(scr, writes ? MASCOT_LIGHT : MASCOT_RYUK, LV_ALIGN_TOP_RIGHT, false);
  }
  load_screen(scr);
}

static void build_edit_bright() {
  lv_obj_t *scr = new_screen("BRIGHTNESS");
  section(scr, "ROTATE TO CHANGE - BACK TO SAVE");
  lv_obj_t *box = content_box(scr);
  lv_obj_t *p = panel(box);
  g_edit_label = make_label(p, "", &lv_font_montserrat_28, C_ACCENT);
  lv_label_set_text_fmt(g_edit_label, "%d%%", g_bright_pct);
  make_label(p, "screen backlight", &lv_font_montserrat_14, C_SUB);
  make_label(box, "needs the BL mod to take effect", &lv_font_unscii_8, C_MUTE);
  load_screen(scr);
  g_edit_val = &g_bright_pct;                    // enable edit mode (render_top cleared it)
  g_edit_min = 10; g_edit_max = 100; g_edit_step = 10;
  g_edit_cb = bright_edit_cb;
}

static void build_edit_power() {
  lv_obj_t *scr = new_screen("INTENSITY");
  section(scr, "ROTATE: LOW / MED / MAX - BACK TO SAVE");
  lv_obj_t *box = content_box(scr);
  lv_obj_t *p = panel(box);
  g_edit_label = make_label(p, pwr_name(g_power_lvl), &lv_font_montserrat_28, C_ACCENT);
  make_label(p, "TX power for every radio", &lv_font_montserrat_14, C_SUB);
  make_label(box, "Max can exceed local power limits", &lv_font_unscii_8, C_AMBER);
  load_screen(scr);
  g_edit_val = &g_power_lvl;
  g_edit_min = 0; g_edit_max = PWR_N - 1; g_edit_step = 1;
  g_edit_cb = power_edit_cb;
}

// ---------------------------------------------------------------- splash
static void splash_advance_cb(lv_timer_t *) {
  if (lock_configured(&g_lock)) nav_lock(); else nav_home();
}

static void build_splash() {
  lv_obj_t *scr = new_screen("");
  lv_obj_t *box = content_box(scr);
  lv_obj_t *wrap = plain(box);
  lv_obj_set_size(wrap, lv_pct(100), lv_pct(100));
  mascot_play(wrap, MASCOT_NOTEBOOK, LV_ALIGN_CENTER, true, 500);
  lv_obj_t *title = make_label(wrap, "EDGEHAX S3-PRO", &lv_font_montserrat_20, C_ACCENT);
  lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -6);
  load_screen(scr);
  lv_timer_t *t = lv_timer_create(splash_advance_cb, 1300, NULL);
  lv_timer_set_repeat_count(t, 1);
}

// ---------------------------------------------------------------- lock screen
// Shared by SCR_LOCK (boot/wake gate, g_locked=true, BACK/HOME disabled — see
// poll_buttons) and SCR_SET_PIN (a normal Settings screen). Both dial a digit
// via the existing encoder edit-mode and confirm it with ACTION, exactly like
// brightness/intensity editing — no new input plumbing.
static uint8_t g_lock_epoch = 0;   // bumped per build_lock(); guards a stale wrong-PIN timer

static void lock_repaint_dots() {
  for (int i = 0; i < 4; i++) {
    if (!g_lock_dots[i]) continue;
    bool filled = i < g_lock.pos;
    lv_obj_set_style_bg_color(g_lock_dots[i], lv_color_hex(filled ? C_ACCENT : C_LINE), 0);
    lv_obj_set_style_bg_opa(g_lock_dots[i], filled ? LV_OPA_COVER : LV_OPA_40, 0);
  }
  if (g_lock_digit_lbl) lv_label_set_text_fmt(g_lock_digit_lbl, "%d", g_lock_digit);
}

static void lock_dial_cb(int v) { g_lock_digit = v; lock_repaint_dots(); }

static void lock_wrong_flash() {
  for (int i = 0; i < 4; i++) {
    if (!g_lock_dots[i]) continue;
    lv_obj_set_style_bg_color(g_lock_dots[i], lv_color_hex(C_RED), 0);
    lv_obj_set_style_bg_opa(g_lock_dots[i], LV_OPA_COVER, 0);
  }
}
static void lock_wrong_reset_cb(lv_timer_t *t) {
  if ((uint8_t)(intptr_t)t->user_data != g_lock_epoch) return;   // screen moved on already
  lock_reset_entry(&g_lock);
  lock_repaint_dots();
}

static void lock_check_or_save() {   // g_action_cb: confirms the dialled digit
  lock_confirm_digit(&g_lock, g_lock_digit);
  g_lock_digit = 0;
  if (!lock_entry_complete(&g_lock)) { lock_repaint_dots(); return; }

  if (g_lock_setting_new) {
    int v = lock_pin_from_digits(g_lock.entry[0], g_lock.entry[1], g_lock.entry[2], g_lock.entry[3]);
    g_lock.pin = (v == 0) ? -1 : v;      // 0000 removes the PIN
    g_cfg_dirty = true;
    lock_reset_entry(&g_lock);
    lock_repaint_dots();
    if (g_lock_status_lbl)
      lv_label_set_text(g_lock_status_lbl, g_lock.pin < 0 ? "PIN removed" : "PIN set");
    mascot_play(lv_scr_act(), MASCOT_MISA, LV_ALIGN_BOTTOM_MID, false);
  } else if (lock_entry_matches(&g_lock)) {
    g_locked = false;
    nav_home();
  } else {
    mascot_play(lv_scr_act(), MASCOT_RYUK, LV_ALIGN_TOP_MID, false);
    lock_wrong_flash();
    lv_timer_t *t = lv_timer_create(lock_wrong_reset_cb, 350, (void *)(intptr_t)g_lock_epoch);
    lv_timer_set_repeat_count(t, 1);
  }
}

static void build_lock(bool setting_new) {
  g_lock_epoch++;
  g_lock_setting_new = setting_new;
  lock_reset_entry(&g_lock);
  g_lock_digit = 0;

  lv_obj_t *scr = new_screen(setting_new ? "SET PIN" : "LOCKED");
  lv_obj_t *box = content_box(scr);

  lv_obj_t *hero = plain(box);
  lv_obj_set_size(hero, lv_pct(100), 64);
  lv_obj_t *l = mascot_play(hero, MASCOT_L, LV_ALIGN_CENTER, true, 260);
  if (l) lv_img_set_zoom(l, 400);   // ~1.6x — hero-sized on the lock screen

  lv_obj_t *p = panel(box);
  lv_obj_t *dots = plain(p);
  lv_obj_set_size(dots, lv_pct(100), 20);
  lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(dots, 14, 0);
  for (int i = 0; i < 4; i++) {
    lv_obj_t *d = lv_obj_create(dots);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, 14, 14);
    lv_obj_set_style_radius(d, 7, 0);
    lv_obj_set_style_border_width(d, 1, 0);
    lv_obj_set_style_border_color(d, lv_color_hex(C_LINE), 0);
    g_lock_dots[i] = d;
  }
  g_lock_digit_lbl = make_label(p, "0", &lv_font_montserrat_28, C_ACCENT);
  g_lock_status_lbl = make_label(p, setting_new ? "0000 removes the PIN" : "enter PIN",
                                 &lv_font_montserrat_14, C_SUB);
  make_label(box, "rotate = digit    ACTION = confirm", &lv_font_unscii_8, C_MUTE);
  load_screen(scr);

  lock_repaint_dots();
  g_edit_val = &g_lock_digit;
  g_edit_min = 0; g_edit_max = 9; g_edit_step = 1;
  g_edit_cb = lock_dial_cb;
  g_action_cb = lock_check_or_save;
}

static void build_settings() {
  lv_obj_t *scr = new_screen("SETTINGS");
  lv_obj_t *list = make_list(scr);
  char buf[40];
  snprintf(buf, sizeof(buf), "%d %%  (rotate to change)", g_bright_pct);
  add_row(list, "BRT", C_ACCENT, "Brightness", buf, NULL, 0, 0, nav_code(SCR_EDIT_BRIGHT, 0, 0));
  snprintf(buf, sizeof(buf), "%s  (rotate to change)", pwr_name(g_power_lvl));
  add_row(list, "INT", C_AMBER, "Intensity", buf, NULL, 0, 0, nav_code(SCR_EDIT_POWER, 0, 0));
  snprintf(buf, sizeof(buf), "dim %ds  sleep %ds", DIM_AFTER_MS / 1000, SLEEP_AFTER_MS / 1000);
  add_row(list, "PWR", C_ACCENT, "Sleep timers", buf, NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  add_row(list, "THM", C_ACCENT, "Theme", "Death Note", NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  add_row(list, "PIN", C_ACCENT, "PIN Lock",
          lock_configured(&g_lock) ? "set - enter 0000 to remove" : "not set",
          NULL, 0, 0, nav_code(SCR_SET_PIN, 0, 0));
  if (storage_ready())
    snprintf(buf, sizeof(buf), "SD %lu / %lu MB used", (unsigned long)storage_used_mb(),
             (unsigned long)storage_total_mb());
  else
    snprintf(buf, sizeof(buf), "no card - insert to save");
  add_row(list, "SD", storage_ready() ? C_ACCENT : C_SUB, "Storage", buf, NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  add_row(list, "?",   C_SUB,   "About", "Edgehax S3-PRO  -  fw m2", NULL, 0, 0, nav_code(SCR_SETTINGS, 0, 0));
  load_screen(scr);
}

// ---------------------------------------------------------------- nav engine
static void render_top() {
  g_edit_val = nullptr;                           // leaving any screen exits edit mode
  g_edit_label = nullptr;
  g_action_cb = nullptr;                          // and clears its ACTION handler
  if (g_cfg_dirty) { save_config_now(); g_cfg_dirty = false; }   // persist on exit
  NavEntry &e = nav_stack[nav_depth - 1];
  switch (e.t) {
    case SCR_HOME:        build_home();               break;
    case SCR_AROUND:      build_around();             break;
    case SCR_CATEGORY:    build_category(e.cat);      break;
    case SCR_TOOL:        build_tool(e.cat, e.tool);  break;
    case SCR_SETTINGS:    build_settings();           break;
    case SCR_EDIT_BRIGHT: build_edit_bright();        break;
    case SCR_EDIT_POWER:  build_edit_power();         break;
    case SCR_SPLASH:      build_splash();             break;
    case SCR_LOCK:        build_lock(false);          break;
    case SCR_SET_PIN:     build_lock(true);           break;
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
static void nav_lock() {                            // boot/wake gate — see poll_buttons
  g_locked = true;
  nav_depth = 1;
  nav_stack[0] = { SCR_LOCK, 0, 0 };
  render_top();
}
static void on_action() {                          // context key
  if (g_action_cb) g_action_cb();
  else Serial.println("[ui] ACTION");
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
  lv_style_set_border_color(&st_item_foc, lv_color_hex(C_ACCENT));
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
  bus_locks_init();

  // De-assert all SPI CS pins so uninitialized modules don't corrupt the buses
  pinMode(PIN_SD_CS, OUTPUT); digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_CC1101_1_CS, OUTPUT); digitalWrite(PIN_CC1101_1_CS, HIGH);
  pinMode(PIN_CC1101_2_CS, OUTPUT); digitalWrite(PIN_CC1101_2_CS, HIGH);
  pinMode(PIN_NRF24_1_CS, OUTPUT); digitalWrite(PIN_NRF24_1_CS, HIGH);
  pinMode(PIN_NRF24_2_CS, OUTPUT); digitalWrite(PIN_NRF24_2_CS, HIGH);

  // backlight PWM (no-op until the LED pin is rewired off 3V3 to PIN_BL_PWM)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BL_PWM, BL_LEDC_FREQ, BL_LEDC_BITS);
#else
  ledcSetup(BL_LEDC_CH, BL_LEDC_FREQ, BL_LEDC_BITS);
  ledcAttachPin(PIN_BL_PWM, BL_LEDC_CH);
#endif
  bl_write(bl_user_duty);

  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(true);

  // SD shares SPI-A with the TFT — mount after the display bus is up.
  storage_begin();
  Serial.printf("[sd] %s (%lu/%lu MB)\n", storage_ready() ? "mounted" : "no card",
                (unsigned long)storage_used_mb(), (unsigned long)storage_total_mb());

  // Restore saved prefs (brightness is the runtime-applicable one; timers are
  // compile-time for now). Falls back to defaults when no card / no file.
  char cfgline[72];
  if (storage_load_config(cfgline, sizeof cfgline)) {
    DeviceCfg cfg = {g_bright_pct, DIM_AFTER_MS / 1000, SLEEP_AFTER_MS / 1000, 0, g_power_lvl, g_lock.pin};
    cfg_parse(cfgline, &cfg);
    g_bright_pct = cfg.bright < 10 ? 10 : cfg.bright > 100 ? 100 : cfg.bright;
    apply_brightness(g_bright_pct);
    g_power_lvl = pwr_clamp(cfg.power);
    set_power_level(g_power_lvl);
    g_lock.pin = cfg.pin;
    Serial.printf("[cfg] restored brightness %d%%, intensity %s, pin %s\n",
                  g_bright_pct, pwr_name(g_power_lvl), lock_configured(&g_lock) ? "set" : "none");
  }

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
  Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
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
  nav_push(SCR_SPLASH, 0, 0);                   // boot splash -> lock (if a PIN is set) or home
  Serial.println("[ui] home ready");
}

void loop() {
  lv_timer_handler();
  pm_tick();
  enc_rearm_tick();
  delay(5);
}
