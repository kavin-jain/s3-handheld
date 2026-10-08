# Improvement backlog

Daily-generated, human-reviewed backlog of grounded software-only improvement ideas
for Edgehax S3-PRO. Each entry below was produced by reading the actual source (not
guessed from names) and cites the exact file + function it targets. Entries are
ideas for the owner to triage, not applied changes.

## 2026-10-01

### 1. `tool_nrf_scan` blocks the UI task on first entry into Band Scanner
**File/function:** `src/main.cpp:1141` (`tool_nrf_scan`) calling `src/nrf24_radio.cpp` (`nrf_scan`)
`tool_nrf_scan` is a screen-render function invoked synchronously from `build_tool()`
(`src/main.cpp:2104`) on the single UI task — there is no `lv_timer_create` poll loop
around the call, unlike every other scan-based tool (`wifi_poll_cb`, `ble_poll_cb`,
`camera_poll_cb`, etc.). `nrf_scan()` itself (`src/nrf24_radio.cpp`) sweeps all
`NRF_CHAN` (40) channels with 20 carrier samples each, toggling
`startListening()`/`stopListening()` + a `delayMicroseconds(130)` settle per sample
while holding `spi_b_mutex` — this is exactly the blocking-call-in-render-path
pattern that was already found and fixed twice in `ble_scan`-based tools. It's
gated by the `s_scanned` cache so it only fires once per boot, but that one call
freezes input/animation for the whole sweep with no feedback. Fix by moving the
sweep behind a `lv_timer_create` poll (one channel per tick), same shape as
`tool_ble_scan`.
**Effort:** S

### 2. `nfc_parse_key` can read past the end of the SD-loaded keys.dic buffer
**File/function:** `src/nfc_keys.h` (`nfc_parse_key`), called from `src/mifare.h`
(`mifare_dict_count`), fed by `src/main.cpp:912` (`tool_mifare`, static `dic[2048]`)
`nfc_parse_key(line, out)` unconditionally reads `line[0]` through `line[12]` (13
bytes, computing both `hi` and `lo` for each pair before checking either for -1)
before it has verified that 13 bytes actually remain in the buffer. `mifare_dict_count`
calls it with `line` pointing directly into the middle of the raw text buffer
loaded from `/nfc/keys.dic` via `storage_read_file(..., dic, sizeof dic)` with
`dic` a fixed 2048-byte static array — it does not pass a remaining-length bound,
only a `\0`-terminated pointer. A trailing malformed/partial line (no newline) that
starts within 12 bytes of the end of that 2048-byte array causes `nfc_parse_key`
to read past the array's end. Fix: have `nfc_parse_key` take an explicit `maxlen`
(remaining bytes) and bound each index against it, or have `mifare_dict_count`
pass `strnlen`-bounded sub-strings.
**Effort:** S

### 3. Flipper `.ir` SD records are counted but never actually loaded/sent
**File/function:** `src/flipper_ir.h` (`flipper_ir_parse`, `flipper_ir_at`), IR
browser at `src/main.cpp:1360` (`tool_ir_db`-style brand/IR screen)
`flipper_ir_parse`/`flipper_ir_at`/`flipper_ir_count` are fully implemented and
unit-tested (`test/test_flipper_ir.cpp`), but a repo-wide search shows they are
never called from `main.cpp` — the only SD-IRDB integration point
(`src/main.cpp:1360`-`1361`) calls `storage_count_files("/ir", ".ir")` purely to
print a count ("+ SD IRDB: N .ir files"). There's no code path that opens a `.ir`
file, indexes into it with `flipper_ir_at`, and hands the parsed record to
`ir_send_flipper`. The advertised SD-IRDB feature is currently inert beyond the
file counter. Wiring it up is the natural next step toward closing the open
BRINGUP.md IRDB item.
**Effort:** M

### 4. NRF24 band-scan sweep runs on the same task it blocks — no second-core offload
**File/function:** `src/nrf24_radio.cpp` (`nrf_scan`), `src/main.cpp` (`setup()`/`loop()`)
Nothing in the firmware uses `xTaskCreatePinnedToCore` — `setup()`/`loop()` is the
only application task, and every radio sweep (including the `nrf_scan` 40-channel
carrier sweep above) runs inline on it. The ESP32-S3's second core sits unused for
all app logic. Once item 1 moves `nrf_scan` off the render path, the better fix is
a small dedicated FreeRTOS task pinned to core 0 that owns the SPI-B radio sweeps
(`nrf_scan`, `cc1101_sweep`) and publishes results into the existing cached
globals (`s_counts`/`s_scanned`), polled by the UI's `lv_timer`. That removes the
scan latency from the UI task entirely instead of just chunking it.
**Effort:** M

## 2026-10-02

### 1. A corrupted/out-of-range `pin=` in config.txt can permanently brick the PIN lock
**File/function:** `src/config.h` (`cfg_parse`/`cfg_int_after`), consumed unchecked at
`src/main.cpp:2406` (`setup()`'s config-restore block); compared in `src/ui_lock.h`
(`lock_entry_matches`/`lock_pin_from_digits`)
`cfg_int_after` reads the `pin=` value out of `/config.txt` with plain `atoi` and no
range check. `setup()` loads it straight into `g_lock.pin = cfg.pin;` with no
clamping (unlike `g_bright_pct`, which is clamped to 10-100 two lines above, and
`g_power_lvl`, which goes through `pwr_clamp`). `lock_configured()` only checks
`pin >= 0`, so any out-of-range value (e.g. a flipped bit making `pin=99999` or
`pin=10000`) is accepted as "configured". But `lock_pin_from_digits` can only ever
produce 0-9999 from the 4-digit keypad entry in `lock_entry_matches`, so a PIN
outside that range can never be matched by any real keypad entry — the owner is
locked out of their own device until they pull the SD card and hand-edit/delete
`config.txt`. Fix: clamp `cfg.pin` to `-1` or `0..9999` (same pattern already used
for brightness/power) before assigning it to `g_lock.pin`.
**Effort:** S

### 2. DuckyScript "load a .txt from SD" is advertised but never implemented — only canned gag lines run
**File/function:** `src/main.cpp:1282` (`tool_badusb`), `src/badusb.cpp:33`
(`badusb_run_line`), `src/ducky.h` (`ducky_parse`)
`tool_badusb`'s screen text literally says "load a .txt from SD, then run", but a
repo-wide search shows `ducky_parse()` has exactly one caller — `badusb_run_line`
in `src/badusb.cpp` — and `badusb_run_line` itself is only ever invoked from
`src/main.cpp:1530` with a fixed line out of the built-in `GAGS[]` table
(`src/gags.h`), inside `tool_usbgag`/`tool_badusb`'s own ACTION handler. There is
no code path anywhere that opens an actual SD file (e.g. under `/ducky`), reads it
line-by-line, and feeds each line through `badusb_run_line`/`ducky_parse` — the
one advertised "real" payload feature of BadUSB/HID is entirely absent; only the
canned pranks in `gags.h` work. Fix: add a `/ducky/*.txt` file picker (same shape
as the existing SD-backed tools) that streams each line through the existing,
already-tested `ducky_parse`/`badusb_run_line` pair.
**Effort:** M

### 3. `src/main.cpp` has grown to 2483 lines holding every screen, tool handler, and state machine
**File/function:** `src/main.cpp` (whole file — ~60 `tool_*` render functions,
`build_tool`/`build_screen`/nav plus the PM/lock/encoder state machines all in one
translation unit)
The file mixes unrelated concerns with no internal split: screen/nav plumbing
(`build_tool`, `new_screen`, `load_screen`), the power-management state machine
(`pm_tick`/`pm_wake`), the lock screen glue, the encoder ISR, and all ~60 per-tool
render callbacks (`tool_wifi_scan`, `tool_mifare`, `tool_badusb`, etc.) are one
5700-line-project's single largest file by a factor of 15 over the next largest
(`src/flipper_ir.h` at 107 lines). That makes the diff surface for any one-tool
change (like items 1-2 above and the 2026-10-01 entries) touch a file every
other change also touches, and makes it hard to see which `tool_*` functions are
missing their `lv_timer_create` poll pattern at a glance. Fix: split per-category
render functions out into `src/screens_<category>.cpp` (wifi, nfc, subghz, nrf24,
badusb, etc.), keeping `main.cpp` to `setup()`/`loop()`, nav, and the PM/lock
state machines.
**Effort:** L

### 4. `ui_lock.h`'s PIN state machine has no host unit test despite being pure logic
**File/function:** `src/ui_lock.h` (`lock_confirm_digit`, `lock_entry_matches`,
`lock_pin_from_digits`, `lock_configured`) — no `test/test_ui_lock.cpp`
`ui_lock.h`'s own header comment says it's "host-testable, no LVGL/Arduino
dependency", the same claim every other pure-logic header in `src/` makes, and
every one of those (`config.h` → `test/test_config.cpp`, `df_logic.h` →
`test/test_df_logic.cpp`, `ac_state.h` → `test/test_ac_state.cpp`, etc.) has a
matching host test — `ui_lock.h` is the one exception. It's also the file that
would have caught item 1 above: a test asserting `lock_entry_matches` can never
be true for `pin` outside `0..9999` (or for `pin == -1`) would have surfaced the
missing clamp at the `cfg_parse` call site. Fix: add `test/test_ui_lock.cpp`
covering digit entry/reset/match, including the out-of-range-`pin` case.
**Effort:** S

## 2026-10-04

Note: re-checking the 2026-10-01/02 entries against current `origin/main`, items
about `tool_nrf_scan`, `nfc_parse_key`, the `.ir`/DuckyScript SD wiring and
`ui_lock` tests have since been fixed; they are not repeated below.

### 1. `DELAY` in an SD payload is unclamped and the whole script runs on the UI task
**File/function:** `src/badusb.cpp:badusb_run_line` (`case DK_DELAY`), `src/main.cpp:ducky_run_action`
`badusb_run_line` does `delay(atoi(arg))` on the raw text after `DELAY`, with no
clamp. `delay()` takes a `uint32_t`, so a payload line like `DELAY -1` (or a
typo/garbled SD line) becomes a ~49-day block. `ducky_run_action` also loops over
every line synchronously from the ACTION handler, so the UI, button polling and
the PM sleep timer are frozen for the whole script with no way to abort. The
2048-byte `text[]` buffer also silently truncates longer payloads mid-line.
Fix: clamp the delay to 0..~10000 ms (host-testable in `ducky.h`) and step one
line per `lv_timer` tick so rotate/click can cancel.
**Effort:** S

### 2. `storage_save_config` rewrites `/config.txt` in place, so a power loss can wipe the PIN and settings
**File/function:** `src/storage.cpp:storage_save_config`, called by `src/main.cpp:save_config_now`
`storage_save_config` opens `/config.txt` with `FILE_WRITE` (truncate) and then
prints the new line. A brownout or battery cut between the truncate and the
`print` leaves an empty/partial file, and `save_config_now` serialises the lock
PIN along with brightness, timers and power level. Fix: write `/config.tmp`, then
`SD.remove` + `SD.rename` over `/config.txt`, and on load fall back to the tmp
file if the main one is empty.
**Effort:** S

### 3. Every encoder tick on the BadUSB and IR screens re-walks the SD directory, and file reads are byte-at-a-time
**File/function:** `src/storage.cpp:storage_nth_file` / `storage_count_files` / `storage_read_file`; callers `src/main.cpp:ducky_paint`, `ir_brand_edit_cb`
`ducky_paint` (the `g_edit_cb`) and the SD branch of `ir_brand_edit_cb` each call
`storage_nth_file`, which opens the folder and iterates `openNextFile()` up to
`idx` while holding `spi_a_mutex`, on the UI task that shares SPI-A with the TFT.
`storage_read_file` also reads with one `f.read()` call per byte. Fix: cache the
file names once on screen entry (small fixed `char[N][32]`), and use
`f.read(buf, n)` for the read loop.
**Effort:** S

### 4. TV-B-Gone has only 5 codes and blasts them on the UI task
**File/function:** `src/tvbgone.h:TVB_CODES`, `src/tvbgone.cpp:tvbgone_fire_all`, `src/main.cpp:tvb_fire_action`
`TVB_CODES` holds 5 entries (Samsung, LG, Sony, generic NEC, Philips), and the
file comment itself calls it a starter table. `tvbgone_fire_all` loops with
`delay(gap_ms)` between sends, called directly from `tvb_fire_action`, so the
screen is frozen for the whole blast and grows with every code added. Since
`flipper_ir_at`/`ir_send_flipper` already work, the blast could also walk a
`/ir/tv*.ir` file from SD, one record per `lv_timer` tick.
**Effort:** M

### 5. SD `.ir` files only ever send record 0, and files over 2 KB are cut off
**File/function:** `src/main.cpp:ir_blast_action`, `src/flipper_ir.h:flipper_ir_count`
`ir_blast_action` loads the file into a static `text[2048]` and calls
`flipper_ir_at(text, 0, &fp)`, so only the first record is reachable, and the UI
label says "click = blast first record". `flipper_ir_count` exists but has no
caller in `src/*.cpp`/`main.cpp`, and the `.ir` text that falls beyond byte 2047
is dropped by `storage_read_file`. Flipper-IRDB remotes usually hold many
buttons, so most of each file is unusable. Fix: rotate through records with the
encoder (`flipper_ir_count` for the range, show the record name), and read the
file in chunks or enlarge the buffer from PSRAM.
**Effort:** M

## 2026-10-07

### 1. WiFi > Deauth (authorized) screen never calls the actual TX function
**File/function:** `src/main.cpp:2208` (`tool_deauth_atk`), vs. `src/deauth_detect.cpp:43`
(`wifi_deauth_tx`)
`tool_deauth_atk` builds one demo `deauth_frame(bcast, bssid, 7, f)` purely to show
on screen, labels itself "pick AP + client, click to send - TX bring-up", but sets
neither `g_action_cb` nor `g_click_cb` — there is no handler at all on this screen.
A repo-wide search confirms `wifi_deauth_tx()` (the fully-implemented, bidirectional
deauth+disassoc sender in `deauth_detect.cpp`, already covered by
`test/test_deauth_frame.cpp` for its frame builders) has zero callers anywhere in
`src/`. This is the same class of "advertised but not wired" gap the 2026-10-02/04
entries found and fixed for DuckyScript/.ir records/TV-B-Gone, just not caught in
that audit. Fix: wire a click handler that calls `wifi_deauth_tx` against the
currently-selected/demo AP+client, same shape as `subghz_replay_action`.
**Effort:** S

### 2. Mifare crack screen is a pure demo — `nfc_crack_block` (the real auth loop) has no caller
**File/function:** `src/main.cpp:1013` (`tool_mifare`), vs. `src/nfc_pn532.cpp:49`
(`nfc_crack_block`)
`tool_mifare` always shows a hardcoded `found[6] = {0xFF...}` key and never reads a
real card — it only counts how many keys are in `/nfc/keys.dic`
(`mifare_dict_count`) and prints `mifare_key_name(found)` against that constant.
`nfc_crack_block(uid, uidLen, block, keyType, dictPath, outKey)` — which actually
opens the SD dictionary, tries each key via `nfc_auth_block` against a real card,
and returns the matching key — is fully implemented in `nfc_pn532.cpp`/`.h` but a
repo-wide search shows it is never called from `main.cpp` or anywhere else. Unlike
`tool_nfc_read` (which already calls `nfc_read_uid` for a live UID), this screen
doesn't even attempt a live read. Fix: call `nfc_read_uid` then `nfc_crack_block`
against `/nfc/keys.dic` (falling back to the built-in `MIFARE_DEFAULT_KEYS` table)
behind an `lv_timer` poll, same shape as `tool_nfc_read`'s `nfc_poll_cb`.
**Effort:** M

## 2026-10-05

### 1. Bug Sweep promises "Press action to sweep again" but sets no action handler, and most of its band list is outside the CC1101's range
**File/function:** `src/main.cpp:tool_audiobug` / `audiobug_timer_cb`, `s_bug_freqs[]`
`audiobug_timer_cb` pauses its timer once the 11-point sweep finishes and paints
"Press action to sweep again", but `tool_audiobug` never assigns `g_action_cb`
(the only handler it sets is `g_cleanup_cb`), so ACTION does nothing and the sweep
cannot be re-run without leaving the screen. Separately, `s_bug_freqs[]` includes
88 / 92.5 / 96.5 / 102.1 / 107.9 / 144 / 155 / 168 MHz, which are below the CC1101
datasheet bands (300-348, 387-464, 779-928 MHz); the RSSI read for those entries
via `cc1101_rssi_at` is unlikely to mean anything, yet the result screen still
labels hits "FM covert mic" / "VHF bug" via `bug_band`. (I could not check how the
SmartRC lib's `setMHZ` treats out-of-band values: it is not vendored in `lib/`.)
Fix: set `g_action_cb` to reset `s_bug_idx` and `lv_timer_resume`, and restrict
the sweep to in-band points (or label the others "not covered by CC1101").
**Effort:** S

### 2. GPIO Play's safety gate only blocks flash/PSRAM pins, so it can drive pins the board itself uses
**File/function:** `src/gpio_util.h:gpio_usable`, `src/main.cpp:gpio_toggle_action`
`gpio_usable` rejects only 26-37 and non-existent pins. `gpio_toggle_action` then
does `pinMode(g_gpio_pin, OUTPUT)` + `digitalWrite` on anything else, including
pins `include/pins.h` assigns to live peripherals: I2C SDA/SCL (1, 2), MCP INT (3),
SPI-B SCLK/MOSI/MISO (4-6), SD SPI-A (10-13), TFT DC/RST/CS (14, 21, 41) and the
encoder (38, 39). Selecting one and pressing ACTION turns a live bus line into a
GPIO output, which can hang the display, buttons or SD until reboot. The screen
shows "safe to drive" for all of them. Fix: add a `gpio_in_use(pin)` table derived
from `pins.h` (host-testable in `gpio_util.h`) and show "IN USE" instead.
**Effort:** S

### 3. Firmware Dump only ever writes the first 64 KB, while BRINGUP.md expects a full chip dump
**File/function:** `src/main.cpp:fwdump_action` (`FWDUMP_BYTES`), `docs/BRINGUP.md:153`
`fwdump_action` allocates a 64 KB PSRAM buffer, does one `esp_flash_read` from
offset 0 and a single `storage_save`; the code comment notes `storage_save` is
one-shot per file. The tool screen says "dump first 64KB", but BRINGUP.md's
checklist item reads "full chip dumps to dump.bin on SD". 64 KB from offset 0 is
bootloader + partition table territory, so the app image and NVS are never
captured. Fix: add an append mode to `storage.cpp` (open once, write 64 KB chunks
for `jedec_capacity_bytes` total, one chunk per `lv_timer` tick with a progress
label), or reword the BRINGUP item to match.
**Effort:** M

## 2026-10-08

### 1. Three separate 2 KB static scratch buffers permanently reserve ~6 KB of RAM for mutually-exclusive one-shot reads
**File/function:** `src/main.cpp:1020` (`tool_mifare`'s `static char dic[2048]`),
`src/main.cpp:1480` (`ducky_run_action`'s `static char text[2048]`), `src/main.cpp:1597`
(`ir_blast_action`'s `static char text[2048]`)
Each of these three functions declares its own file-scope `static char ...[2048]`
buffer purely to hold one `storage_read_file()` result (a `keys.dic`, a `.txt`
DuckyScript, or a `.ir` record) for the duration of that single call. Because
the UI is single-task and only one tool screen/action can ever be active at a
time (`build_tool()` tears down the previous screen before the next renders,
and `g_action_cb`/`g_click_cb` only ever point at one handler), these three
buffers are never live simultaneously, yet each is a distinct named `static`
so the linker reserves all three in BSS permanently — 6144 bytes that could be
one shared 2048-byte scratch buffer. On a chip where flash/PSRAM already carry
the LVGL framebuffers and `g_ndef_tag`/mascot sprite data, trimming 4 KB of
dead-weight static RAM is free. Fix: declare one `static char g_sd_scratch[2048];`
at file scope and have all three call sites use it instead of their own copy.
**Effort:** S

### 2. The covert-bug sweep's "waves" animation forces a full resize+re-layout on every single animation frame
**File/function:** `src/ui_anim.cpp` (`waves_anim_cb`, called by `ui_anim_waves_create`),
used by `src/main.cpp:1929` (`tool_audiobug`, during the live 50 ms `audiobug_timer_cb` sweep)
`waves_anim_cb` is the `lv_anim` exec callback for each of the three expanding-ring
arcs `ui_anim_waves_create` spawns; it runs on every animation tick (LVGL's
default animation refresh, effectively every display refresh) and calls
`lv_obj_set_size(arc, v, v)` followed by `lv_obj_center(arc)` each time, i.e. a
full size + position recompute (and the invalidate/redraw that `lv_obj_set_size`
triggers) for all three concurrently-animating arcs, every frame, for as long as
the Bug Sweep screen's sweep runs. `ui_anim_radar_create`'s equivalent callback,
by contrast, only ever adjusts the arc's start/end angles
(`lv_arc_set_bg_angles`), which LVGL can redraw without a layout pass — the waves
animation is the one path doing the heavier operation per frame, on the one
screen (`tool_audiobug`) that already runs a live polling timer for several
seconds. Fix: pre-size the three arcs once and animate an `lv_obj_set_style_*`
transform/opacity-only property instead of calling `lv_obj_set_size`/`lv_obj_center`
every tick (e.g. a scale transform via `lv_obj_set_style_transform_zoom`, which
skips layout).
**Effort:** S
