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

## 2026-10-09

### 1. CC1101 is left in RX (never idled) after Frequency finder / Direction finder / Bug Sweep
**File/function:** `src/radio_cc1101.cpp:cc1101_rssi_at`; callers `src/main.cpp:freq_paint`, `tool_df`, `audiobug_timer_cb`
`cc1101_rssi_at` does `setMHZ` + `SetRx()` and returns without ever calling `setSidle()`.
The only `setSidle()` in the firmware is in `subghz_capture_end`/`subghz_replay`, and
`main.cpp` has no other idle call (grep: `setSidle` appears only in `radio_cc1101.cpp`).
So after leaving any RSSI-only screen the radio keeps receiving (roughly 15 mA class draw)
through `PM_DIM` and into `esp_light_sleep_start()` in `pm_tick`, which undercuts the
power-management work. Fix: add `cc1101_idle()` (SpiStrobe SIDLE under `spi_b_mutex`) and call
it from a `g_cleanup_cb` on those three screens and at the top of the `pm_tick` sleep branch.
**Effort:** S

### 2. Radio task wakes every 20 ms forever just to check one flag
**File/function:** `src/radio_task.cpp:radio_task_fn`, `src/nrf24_radio.cpp:nrf_scan_request` / `nrf_scan_service`
`radio_task_fn` loops `nrf_scan_service(); vTaskDelay(pdMS_TO_TICKS(20));`, and
`nrf_scan_service` returns immediately unless the `s_scan_requested` flag is set. That is
50 wakeups/s on core 0 for the whole uptime to service a request that comes from a user click,
and a new request can wait up to 20 ms. Fix: store the task handle (the `xTaskCreatePinnedToCore`
call currently passes `NULL`), have `nrf_scan_request` call `xTaskNotifyGive`, and block in
`ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` in the loop. Future `*_service()` calls can share the
same notification.
**Effort:** S

### 3. `storage_save` and `storage_save_config` report success without checking the write result
**File/function:** `src/storage.cpp:storage_save` (`f.write(data, len)`), `storage_save_config` (`f.print(text)`)
Both ignore the return value of the write. On a full or write-protected card, `f.write` returns
fewer bytes than requested (or 0), yet `storage_save` still returns the new path, so callers
such as `fwdump_action` and the capture saves show "saved" for a truncated or empty file.
`storage_save_config` returns `true` the same way. Fix: compare the returned count with `len`
(and `strlen(text)`), `SD.remove` the partial file, and return `""`/`false` so the UI can say
"SD write failed".
**Effort:** S

### 4. NRF24 absence is latched at first touch, so a module that wasn't ready then stays "not detected" until reboot
**File/function:** `src/nrf24_radio.cpp:ensure` (`s_begun = true` set before `radio.begin(&SPI)`), `nrf_present`
`ensure()` sets `s_begun = true` first and then evaluates `radio.begin() && isChipConnected()`
exactly once. `nrf_present()` (called from `tool_nrf_scan` and `nrf_rescan_action`) never retries,
so a flaky first probe (module powered late, loose jumper reseated, SPI-B busy during bring-up)
makes every NRF24 screen fall into its "demo - NRF24 not detected" branch until power cycle.
BRINGUP.md already expects bring-up on a breadboard. Fix: only latch `s_begun` on success, or
retry the probe at most once every few seconds when `!s_present`.
**Effort:** S

### 5. LVGL uses one 320x40 draw buffer and a synchronous, non-DMA `pushColors` flush
**File/function:** `src/main.cpp:flush_cb`, `setup()` (`lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCR_W * 40)`), `platformio.ini` TFT_eSPI flags
`flush_cb` pushes each chunk with `tft.pushColors` while holding `spi_a_mutex`, and the second
buffer argument to `lv_disp_draw_buf_init` is `NULL`. LVGL therefore cannot render the next
strip while the previous one is on the wire; a full-screen transition (the 150 ms MOVE_* anim in
`nav`) renders and transfers strictly in series. `platformio.ini` sets 40 MHz SPI but no DMA
use anywhere (grep for `initDMA`/`pushPixelsDMA` is empty), while the board has 8 MB PSRAM
(and internal RAM for a second 25 KB buffer). Fix: add a second `buf2`, call `tft.initDMA()`
in `setup()`, push with `pushPixelsDMA` and call `lv_disp_flush_ready` once the transfer
completes. SD access on the shared bus must wait for DMA done, so keep the `spi_a_mutex`
held until then. Needs on-hardware verification of the SPI-A sharing.
**Effort:** M

## 2026-10-10

### 1. `pm_tick()` can drop the device into light sleep while an async tool scan is still mid-flight
**File/function:** `src/main.cpp:pm_tick` (around line 449-482), vs. `g_tool_timer`
`pm_tick()` only looks at `idle = millis() - last_input_ms` against `g_dim_s`/`g_sleep_s`; it
never checks whether `g_tool_timer` (the active poll timer every async scan — `wifi_poll_cb`,
`ble_poll_cb`, `nrf_scan_poll_cb`, `camera_poll_cb`, `ir_poll_cb`, …) is non-null before calling
`esp_light_sleep_start()`. Starting a scan does not reset `last_input_ms` on its own, so a user
who starts a multi-second scan (BLE, WiFi, nRF24 band sweep) and then sets a short `sleep_s`
(the only enforced floor is `dim_s + 5`, via `sanitize_sleep_dim()`) and walks away will have
the chip halt into light sleep mid-scan — freezing whatever's in flight on core 0/the radio
task and leaving the poll timer to resume against stale/incomplete state on wake. Fix: gate the
`PM_DIM -> PM_SLEEP` transition (and ideally the `PM_ACTIVE -> PM_DIM` one) on `g_tool_timer ==
nullptr`, or have each async scan push its own last-activity timestamp.
**Effort:** S

### 2. `nfc_crack_block`'s dictionary read desyncs after any keys.dic line longer than 23 characters
**File/function:** `src/nfc_pn532.cpp:nfc_crack_block`
The read loop is `size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);` with `char
line[24]`, i.e. at most 23 bytes per call. `nfc_parse_key` (`src/nfc_keys.h`) correctly allows
`#`-comment lines of any length, but `readBytesUntil` does **not** consume the rest of an
over-length line once it hits its 23-byte cap — it just returns what it has, leaving the
remaining bytes of that same line (up to and past the real `\n`) sitting in the file. The next
loop iteration's `readBytesUntil` call then reads that leftover tail as if it were the start of
the next line, which `nfc_parse_key` will reject as garbage — silently skipping the dictionary
entry that actually followed the long line. Any keys.dic with a comment/header line over 23
characters (very easy to write by hand) quietes-fails to crack keys that are otherwise present.
Fix: size `line` to the longest realistic dictionary line (comments included) or explicitly
drain the remainder of an over-length line (loop `f.read()` until `'\n'` or EOF) before parsing
the next one.
**Effort:** S

### 3. WiFi scans never call `WiFi.scanDelete()`, leaking the previous scan's heap allocation on every rescan
**File/function:** `src/wifi_scan.cpp:wifi_scan_async` / `wifi_scan_complete`
`wifi_scan_async()` just calls `WiFi.scanNetworks(true)` and `wifi_scan_complete()` just reads
`WiFi.scanComplete()` — `scanDelete()` is never called anywhere in the codebase (confirmed by
grep across `src/`). Every tool that scans WiFi (`tool_wifi_scan`, `tool_camera`,
`tool_tracker_hunt`'s WiFi half, wardrive's recon, etc.) re-triggers `wifi_scan_async()` on
rescan/re-entry per the existing "click = rescan: re-enter the screen" pattern noted at
`src/main.cpp:1170`, without ever releasing the previous scan's internal `wifi_ap_record_t`
array that the IDF Wi-Fi driver keeps allocated until `esp_wifi_scan_get_ap_records` /
`scanDelete` frees it. Repeated rescanning (the normal way these tools are used) steadily grows
heap usage for the life of the session. Fix: call `WiFi.scanDelete()` once the UI has read out
the records it needs (right after building the result list in each `*_poll_cb`), before the
next `wifi_scan_async()` call.
**Effort:** S

## 2026-10-11

### 1. `task_parse` can read one byte past a too-short line before checking it's there
**File/function:** `src/tasks.h:task_parse` (line 11), fed by `src/main.cpp:1408`
(`tool_tasks`, SD-loaded `/me/tasks.txt`)
`task_parse`'s guard is `if (!line || line[0] != '[' || line[2] != ']') return false;` —
it indexes `line[2]` as soon as `line[0] == '['`, without first confirming `line[1]`
is non-NUL. For the 2-byte string `"["` (`line[0]='['`, `line[1]='\0'`), that reads
`line[2]`, one byte past the string's own allocation, before the length is known to
be long enough. The one call site in `main.cpp:1419-1423` happens to copy each line
into a fixed `char line[64]` stack buffer first, so today's only caller is safe by
accident (the extra byte is still inside that 64-byte array, just uninitialized
stack garbage) — but the function itself, called directly (as a unit test or any
future caller would, exactly like `nfc_parse_key` before its fix), has the same
reads-a-fixed-offset-before-checking-length shape that bug had. Fix: check
`line[1] == 0` (reject) before indexing `line[2]`, or check `!line[1] || !line[2]`
up front.
**Effort:** S

### 2. "Drone spotter" never calls the OpenDroneID decoder it ships and tests — it only keyword-matches the BLE device name
**File/function:** `src/main.cpp:2003` (`droneid_timer_cb`) vs. `src/droneid.h`
(`odid_msg_type`, `le_i32`, `odid_coord`, `odid_basic_id`), tested by
`test/test_droneid.cpp`
`droneid.h`'s own header comment says it decodes "the operator/aircraft id and the
live GPS position" out of an OpenDroneID Remote-ID beacon, and its four decode
functions are fully implemented and unit-tested. But `droneid_timer_cb` (the only
code path behind `tool_droneid`) never calls any of them — a repo-wide grep for
`le_i32`/`odid_basic_id`/`odid_msg_type` outside `droneid.h`/`test/` has zero hits.
Instead it does a plain BLE-name substring match (`ci_contains(n, "DJI")` /
`"Drone"` / `"Mavic"`) against whatever `ble_scan_async` turns up and calls that
"drone broadcast nearby" — a much weaker signal than decoding the actual Remote-ID
payload (which carries a real operator/aircraft ID and GPS fix, not just a product
name string many drones won't even advertise). Unlike the BRINGUP.md checklist item
for this tool (which is about verifying RF reception on hardware), this gap needs no
hardware: the already-tested decoder just needs to be run against the raw BLE
advertisement bytes (the ODID Message Pack is carried in a BLE Service Data AD
structure) instead of the device's advertised name. Fix: in `droneid_timer_cb`,
pull each matched BLE device's raw advertisement payload and feed the ODID Message
Pack through `odid_msg_type`/`odid_basic_id`/`le_i32`+`odid_coord`, same shape as
how `flipper_ir_at` was wired into the IR browser.
**Effort:** M

### 3. Four tool screens duplicate the same "BLE async scan + poll timer + render" boilerplate with no shared helper
**File/function:** `src/main.cpp` — `skimmer_timer_cb` (line 1950) / `tool_skimmer`
(1981), `droneid_timer_cb` (2003) / `tool_droneid` (2032), `wof_poll_cb` (2308) /
`tool_wof` (2316), and the equivalent pair behind `tool_tracker_hunt` (2332, same
`ble_scan_complete()`-poll shape at line 2369)
Each of these four screens independently repeats the same five-step pattern: call
`ble_scan_async(N)`, start an `lv_timer_create(..., 200/500, ...)` poll callback,
have that callback early-return while `!ble_scan_complete()`, delete/pause the
timer and set `g_tool_timer`/`s_active_tool_timer` to null once it fires, then loop
`for (i = 0; i < ble_count(); i++)` matching each device against a predicate
(`is_skimmer_name`, a DJI/Drone/Mavic substring check, a Flipper/pwnagotchi
heuristic, `ble_tracker_brand`). The only things that differ between the four are
the predicate and the result labels — the scan-start/poll/teardown plumbing is
copy-pasted each time (and already had to be fixed identically in more than one of
them for the same leaked-timer/dead-ACTION class of bug noted in BRINGUP.md). Fix:
factor the common shape into one helper, e.g. `ble_scan_and_poll(seconds, lv_timer_cb_t
on_done)`, that owns the timer lifecycle and calls back once `ble_scan_complete()`
is true, leaving each tool to supply only its predicate/render step.
**Effort:** M

## 2026-10-12

### 1. `nfc_crack_block`'s SD read is the one SD access site in the codebase that never takes `spi_a_mutex`
**File/function:** `src/nfc_pn532.cpp:nfc_crack_block` (`SD.open(dictPath)` / `f.readBytesUntil`,
line 52-58), vs. every SD call in `src/storage.cpp` (`storage_begin`, `storage_save`,
`storage_save_config`, `storage_nth_file`, `storage_read_file`, `storage_load_config`),
which all wrap their `SD.open`/read/write in `xSemaphoreTakeRecursive(spi_a_mutex, ...)`.
SD and the TFT share SPI-A (`storage.cpp`'s own header comment documents this and
gates every other SD call on the mutex for exactly that reason), but `nfc_crack_block`
dials the dictionary file straight off `SD.open()` with no mutex at all. Any
`lv_timer_handler()` redraw (TFT `flush_cb`, which also takes `spi_a_mutex`) that lands
mid-crack races an un-synchronized SD transaction against an in-flight display SPI
transaction on the same bus. Fix: wrap the open/read/close in `nfc_crack_block` with the
same `xSemaphoreTakeRecursive(spi_a_mutex, portMAX_DELAY)` / give pattern used throughout
`storage.cpp`.
**Effort:** S

### 2. The second-core radio task is never registered with the task watchdog that `loop()` already uses
**File/function:** `src/main.cpp:setup()` (`esp_task_wdt_init` / `esp_task_wdt_add(NULL)`,
~line 2709-2712) vs. `src/radio_task.cpp:radio_task_start` /`radio_task_fn`
`esp_task_wdt_add(NULL)` in `setup()` subscribes only the calling task — the Arduino
loop task on core 1 — and `loop()` is the only place that calls `esp_task_wdt_reset()`.
`radio_task_start()` is called right after, from that same core-1 context, but it only
spins up `radio_task_fn` on core 0 via `xTaskCreatePinnedToCore`; nothing ever calls
`esp_task_wdt_add()` for that new task handle, and `radio_task_fn`'s own loop
(`nrf_scan_service(); vTaskDelay(20)`) never resets anything. So the watchdog this
project already added covers the UI/main task but has no visibility at all into the
core-0 radio task — if `nrf_scan_service()` or a future SPI-B user on that task ever
deadlocks on `spi_b_mutex` (e.g. against a UI-task holder that itself stalls), the WDT
that's supposed to panic+reboot the device never fires, since it was never watching that
task. Fix: capture the `TaskHandle_t` from `xTaskCreatePinnedToCore` and add it with
`esp_task_wdt_add(handle)`, resetting it from inside `radio_task_fn`'s own loop.
**Effort:** S

## 2026-10-13

### 1. USB host is a permanently hardcoded fake MBR — no real USB MSC driver is ever wired in
**File/function:** `src/main.cpp:1374` (`tool_usbhost`)
`tool_usbhost` builds a 512-byte MBR buffer entirely in the function body
(`uint8_t mbr[512] = {0}; ... e[4] = 0x0C; e[9] = 0x08; ...` — a hand-crafted single
FAT32 partition starting at LBA 2048) and feeds that same local array straight into
`mbr_valid()`/`mbr_part_type()`/`mbr_part_sectors()`. There is no USB Host stack call
anywhere in this function, no real MSC bulk-only transport read, and no SD-card
passthrough despite the screen's own copy promising "browse & copy files to SD" —
every visit to this screen shows the identical "drive mounted / P1 FAT32 1MB" result
whether or not anything is even plugged into the USB-C port. Of all 61 tools this is
the one with literally zero hardware path behind its headline claim (even the other
bring-up demo screens read *something* live, e.g. `cc1101_rssi_at`/`nfc.begin()`).
Needs the Arduino-ESP32 `USBHost`/MSC class wired to actually enumerate a connected
drive and read its real MBR before this can leave "bring-up" status.
**Effort:** L

### 2. `nfc_write_ndef` never checks the write stays inside the tag's actual writable page range
**File/function:** `src/nfc_pn532.cpp:37` (`nfc_write_ndef`)
The function loops `for (size_t off = 0, pg = 4; off < len; off += 4, pg++)` and calls
`nfc.ntag2xx_WritePage((uint8_t)pg, page)` for as many 4-byte pages as `len` needs,
with no upper bound on `pg` at all. A real NTAG213 (the cheapest/most common blank
NTAG the universal-remote-sized URL from `tool_ndef`/`ndef_uri_record` would be written
to) only has user-memory pages 4-39 (144 bytes); NTAG215/216 go further but still end.
Past the last user page, `ntag2xx_WritePage` is writing into the tag's lock bytes,
CC, or configuration pages — depending on the tag and library return behavior this can
either silently no-op, corrupt the tag's own capability container, or write into the
dynamic-lock/config area (making the tag unreadable). There's currently nothing in
this file stopping `tool_ndef` from calling it with a URL long enough to walk off the
end of a real tag; it relies entirely on the PN532 library/tag firmware rejecting the
out-of-range page, which is not guaranteed. Fix: take a `maxPage` (or tag-size) arg and
`return false` before writing past it.
**Effort:** S

### 3. Mascot sprite frames are stored as 4-bytes-per-pixel RGBA for art that only uses a handful of colors
**File/function:** `src/sprites_ryuk.c`, `src/sprites_misa.c`, `src/sprites_light.c`,
`src/sprites_ir_beam.c`, `src/sprites_nfc_card.c` (each frame's `_map[]` array +
`lv_img_dsc_t` with `.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA`)
Every mascot frame (28x28px, e.g. `ryuk_0`/`ryuk_1` in `sprites_ryuk.c`) is declared
`LV_IMG_CF_TRUE_COLOR_ALPHA`, LVGL's 4-bytes-per-pixel (BGRA) format — 28*28*4 = 3136
bytes per frame, for pixel art that (reading `ryuk_0_map`/`ryuk_1_map` directly) uses
only ~5 distinct RGB values plus fully-transparent `0,0,0,0` background, repeated in
solid runs. Across the 16 total `TRUE_COLOR_ALPHA`-tagged frames in `src/sprites_*.c`
that's roughly 48-50 KB of flash (`.rodata`) that an `LV_IMG_CF_INDEXED_4BIT` (16-color
palette + 1 alpha bit per pixel, or `INDEXED_8BIT` for safety margin) encoding would
cut by 70-85%, freeing flash for the IRDB/AC-brand tables or future features without
any visual difference, since none of these sprites exceed a 16-color palette.
**Effort:** M

## 2026-10-14

### 1. Bus Pirate's I2C scan runs its full blocking probe loop directly in the render path
`src/main.cpp:i2c_scan_paint` (called from `tool_buspirate`, both on screen-entry and on every ACTION rescan) loops `Wire.beginTransmission`/`endTransmission` synchronously over all 112 addresses (0x08-0x77) with no `lv_timer_create` poll wrapper around it, unlike every other scan-style tool in the file. Each `endTransmission()` can take several ms when a device NAKs or the bus is busy, so this is the same "blocking call with no poll loop" pattern already fixed twice in `ble_scan`-based tools, just on I2C instead of BLE/radio. Effort: S.

### 2. ESP-NOW mesh screen never refreshes after first paint
`src/main.cpp:tool_espnow` builds its "N messages received" / last-message labels once from `espnow_rx()`/`espnow_last()` on screen entry and installs no `lv_timer_create` poll callback, so an incoming broadcast received while the screen is open never updates `g_espnow_status` or the message labels — the user has to leave and re-enter the screen to see new traffic land. Every other live-data screen in this file (WiFi, BLE, NRF24, tracker, wof) uses a `lv_timer_create(..._poll_cb, ...)` to re-paint; this one doesn't. Effort: S.

### 3. Frequency finder's RSSI reading never ticks on its own — only on encoder rotation
`src/main.cpp:tool_freq_finder`/`freq_paint`: RSSI is sampled once at screen-entry and again only from `g_edit_cb = freq_paint` on each encoder tick (`g_edit_val`/`g_edit_cb` wiring further down in the same function). There's no `lv_timer_create` to re-sample the currently-selected band's RSSI while the user holds still watching a signal — the bar and dBm reading go stale the moment the encoder stops moving, unlike the Bug Sweep or Direction finder screens which keep sampling. Effort: S.

### 4. `radio_task_fn` only services NRF24 — CC1101 RSSI/capture polling still runs entirely on the UI task
`src/radio_task.cpp:radio_task_fn` calls `nrf_scan_service()` every 20ms on core 0, but every CC1101 operation (`cc1101_rssi_at` in `src/radio_cc1101.cpp`, called from `freq_paint`, `tool_bug_sweep`'s `audiobug_timer_cb`, and the sub-GHz capture poll in `src/main.cpp`) still executes synchronously on the LVGL/UI task via `lv_timer_create` callbacks. The comment at the top of `radio_task.cpp` frames this task as the home for "every blocking SPI-B sweep," but CC1101 sweeps were never actually moved there — only NRF24 was. This leaves the same single-core contention for Sub-GHz band scans that the second core was meant to relieve. Effort: M.

### 5. `espnow_msg.h`'s frame functions have no host unit test
`test/` covers every other pure-logic codec in the repo (`test_ducky.cpp`, `test_handshake.cpp`, `test_gatt_uuid.cpp`, etc.) but there is no `test_espnow_msg.cpp` for `src/espnow_msg.h`'s `espnow_frame`/`espnow_parse`, despite both being small `static inline` pure functions (frame-build + XOR-checksum validate/extract) that are exactly the shape the ASan/UBSan host-test harness in `test/run_all.sh` is built for — malformed-length, truncated-buffer and bad-checksum cases are each one-line test cases, the same style that caught the `nfc_parse_key` off-by-one in this suite before. Effort: S.

## 2026-10-15

### 1. Flipper `raw`-type `.ir` records are recognized by the parser but structurally cannot be sent
`src/flipper_ir.h:18` declares `enum IrRecType { IRREC_NONE, IRREC_PARSED, IRREC_RAW }`, and `flipper_ir_parse` (`src/flipper_ir.h`, via `flipper_type` at line 44-47) correctly identifies a `type: raw` record and sets `fp->type = IRREC_RAW`. But the `FlipperIr` struct it fills (`src/flipper_ir.h:20-27`) only has fields for `protocol`/`addr`/`cmd` — there is no field anywhere for the raw timing array or carrier frequency a `type: raw` Flipper record actually carries (e.g. `data: 9000 4500 560 560 ...`). `ir_send_flipper` (`src/ir_remote.cpp:62`) then hard-rejects anything that isn't `IRREC_PARSED`: `if (!s_ready || !fp || fp->type != IRREC_PARSED) return false;`. So even once SD `.ir` records are wired up (see the 2026-10-01 #3 item above), every raw-type record — which is how most AC remotes, projectors, and any protocol IRremoteESP8266 doesn't have a named encoder for are stored in real Flipper-IRDB dumps — will silently fail to send. Fixing this needs a raw-data field on `FlipperIr`, a parser branch that captures the `data:`/`frequency:` lines, and an `irsend.sendRaw()` path in `ir_send_flipper`.
**Effort:** M

### 2. Wardrive tool never scans WiFi or reads GPS — it logs one hardcoded fake row every time
`src/main.cpp:2148` (`tool_wardrive`) calls `wardrive_csv("A4:2B:B0:11:22:33", "linksys", "[WPA2-PSK-CCMP][ESS]", 6, -52, 12.971600, 77.594600, row, sizeof row)` — every argument is a literal constant. There is no call to `wifi_scan_async()`/`wifi_scan_complete()` (the functions `src/wifi_scan.cpp` already exposes and every other WiFi screen uses) and no GPS read anywhere in the function; the labels under it ("1 net - fix 12.9716,77.5946", "wardrive.csv ready to upload") describe this fixed string, not a live capture. Opening the screen or pressing ACTION always "logs" the same fake Bangalore coordinate and the same fake `linksys` AP, regardless of what's actually in the air. The function's own trailing label ("needs GPS fix + SD - scan is bring-up") flags this as known, but the fix is now mechanical: swap in a `wifi_scan_async()`/poll-timer pair (the same shape `tool_wifi_scan` already has) and loop `wardrive_csv()` over the real result count, deferring only the GPS fix itself.
**Effort:** M

### 3. ESP-NOW peer registration failure is silently ignored, so the mesh can report "ready" while broadcast send always fails
`src/espnow_mesh.cpp:39` (`espnow_begin`) calls `esp_now_add_peer(&peer)` and discards its return value, then unconditionally sets `s_on = true` and returns `true`. If that call fails — e.g. `ESP_ERR_ESPNOW_FULL`, `ESP_ERR_ESPNOW_EXIST` from a stale peer table entry surviving a soft reset, or `ESP_ERR_ESPNOW_NOT_INIT` on an init race — `espnow_active()` (checked at `src/main.cpp:1362` before calling `espnow_begin()` again) still reports the mesh as up, and every later `espnow_broadcast()` (`src/espnow_mesh.cpp:48`) calls `esp_now_send(BCAST, f, n)` against a peer that was never actually added, which `esp_now_send` itself will reject. The UI has no way to learn this happened — "ESP-NOW mesh" just looks like it's working while silently dropping every message. Fix: check `esp_now_add_peer`'s return, and only set `s_on = true` (or report it) on success.
**Effort:** S

## 2026-10-16

### 1. Status-bar battery % is a one-shot snapshot and nothing reacts to a low battery
**File/function:** `src/main.cpp:622` (`build_statusbar`), `src/battery.cpp:batt_pct`
`build_statusbar` reads `batt_pct()` once, formats it into a plain label, and is only called while a screen is being built (`src/main.cpp:747`). There is no `lv_timer` that refreshes it. `batt_pct()` has no other caller in the tree (only `src/main.cpp:638`). So on a long-lived screen such as a scan or the idle home screen, the percentage never changes until you navigate away. The icon is also hardcoded to `LV_SYMBOL_BATTERY_FULL`, and `pm_tick` never looks at battery level. A 3.7 V cell can sag into brown-out mid-SD-write with no warning. Fix: add a slow (about 30 s) timer that updates the label text and symbol, plus a low-battery threshold that dims early and warns before saves.
**Effort:** S

### 2. `wifi_save_action` silently truncates the CSV at 512 bytes and does not quote SSIDs
**File/function:** `src/main.cpp:1159` (`wifi_save_action`)
It builds the CSV in a fixed `char body[512]` and stops the loop at `o < sizeof body - 80`, so roughly the first 10-12 networks are saved and the rest are dropped with no indication. The status line still shows the saved path. Rows are written as `%s,%s,%d,%d`, so an SSID containing a comma or newline (both legal) shifts the columns for that row and corrupts the file for any parser. Fix: stream rows to the file in chunks, or at least size `body` from `wifi_count()`. Quote SSIDs with RFC-4180 escaping. A pure `csv_escape` helper in `strutil.h` would be host-testable.
**Effort:** S

### 3. `ir_send_flipper` sends Flipper `NEC42` records as 32-bit NEC
**File/function:** `src/ir_remote.cpp:66` (`ir_send_flipper`)
The branch `strcmp(p,"NEC") || "NECext" || "NEC42"` collapses all three into `irsend.sendNEC(irsend.encodeNEC(a16, c16))`, using only the low 16 bits of address and command. NEC42 is a different, longer frame (13-bit address and command fields). Sending it as a 32-bit NEC frame will not be recognised by the target device and silently reports `true` (success). Fix: drop `NEC42` from this branch so it falls through to `return false` ("not yet mapped"), or build the 42-bit frame explicitly. Either way, add a case to `test/test_flipper_ir.cpp`'s protocol-mapping coverage.
**Effort:** S

### 4. `rcs_decode` has no 32-bit cap, so long captures corrupt the code and replay shifts out of range
**File/function:** `src/subghz_replay.h:77` (`rcs_decode`), consumed by `src/radio_cc1101.cpp:subghz_replay`
`rcs_decode` shifts into a `uint32_t code` for every valid pulse pair (`code = (code << 1) | b`) and counts `bits` with no upper limit, then returns `bits_out = (uint8_t)bits`. `SGX_MAX` is 300 edges, so up to about 149 pairs can be accepted. For a valid frame longer than 32 bits, `code` silently keeps only the last 32 bits while `bits` reports the real length. `subghz_replay` then runs `(code >> i)` with `i` up to `bits-1`, which is undefined behaviour for `uint32_t` when `i >= 32`. `subghz_save_action` also writes a `.sub` file whose `bits:` header does not match its `code:`. Fix: reject (or clamp to) `bits > 32` inside `rcs_decode` and add a >32-bit case to `test/test_subghz_replay.cpp`, which today only covers a 24-bit frame and junk.
**Effort:** S

### 5. `loop()` spins at ~200 Hz regardless of LVGL's next-timer deadline, including while dimmed
**File/function:** `src/main.cpp:2828` (`loop`), `pm_tick` (`src/main.cpp:449`)
`loop()` calls `lv_timer_handler()` and then a fixed `delay(5)`, discarding the return value, which is LVGL's ms-until-next-timer hint. In `PM_DIM` the CPU drops to 80 MHz but the loop still wakes every 5 ms with the screen mostly static. True light sleep only starts after `g_sleep_s`. Fix: use the returned value (clamped, e.g. 5 to 30 ms active and up to 50 ms in `PM_DIM`) as the delay. Keep the cap low enough that `enc_rearm_tick` and the 30 ms `poll_buttons` timer stay responsive.
**Effort:** S

## 2026-10-17

### 1. "Tracker on me?" and Wall of Flipper show any non-empty BLE cache as if it were a fresh scan
**File/function:** `src/main.cpp:2378` (`tool_tracker`), `src/main.cpp:2318` (`tool_wof`), `src/ble_scan.cpp:66` (`ble_count`)
Both handlers do `if (ble_count() > 0) { render(cached); return; }` under a "cached from a recent scan" comment, but `s_count` in `ble_scan.cpp` is never aged or cleared — it keeps whatever the last `scan_done_cb` stored, from any screen. So after one Bluetooth scan, "Tracker on me?" answers instantly from that old data and never starts `ble_scan_async`. For an "am I being followed" check, a result from minutes ago (or another location) is misleading. Fix: record `millis()` in `scan_done_cb`, expose `ble_scan_age_ms()`, and only reuse the cache under ~10 s.
**Effort:** S

### 2. Tracker hunt scores a hardcoded Apple advertisement and never scans
**File/function:** `src/main.cpp:2332` (`tool_tracker_hunt`), `src/ble_track.h` (`ble_tracker_brand`, `ble_company_id`)
`tool_tracker_hunt` builds `demo[] = {0x4C,0x00,0x12,0x19}` and passes it to `ble_company_id`/`ble_tracker_brand`, so the "nearest:" line always reads Apple regardless of what is nearby, and no scan or RSSI is read despite the "RSSI rises as you near it" text. The only caller of these two helpers is this demo — `scan_done_cb` in `ble_scan.cpp` calls just `ble_is_airtag`, so Tile/SmartTag detection promised by the "find AirTag / Tile / SmartTag" label has no live path. Fix: keep company id/brand per device in `ble_scan.cpp`, and give the hunt screen an `lv_timer_create` poll that tracks the strongest tracker's RSSI.
**Effort:** M

### 3. iButton screen reports "crc ok" for a ROM it fabricates itself
**File/function:** `src/main.cpp:2222` (`tool_ibutton`)
The handler fills `rom[]` with constants, computes `rom[7] = onewire_crc8(rom, 7)`, then displays `ibutton_valid(rom) ? "ok" : "bad"` — which can only ever be "ok". Grep finds no 1-Wire driver or pin definition anywhere in `src/`/`include/pins.h`, so nothing is read from hardware. Same demo-as-feature class as the wardrive finding. Fix: until a bit-bang reader exists, show "not implemented" (like `tool_transit`'s absent-hardware branch) instead of a green CRC that looks like a real read.
**Effort:** S (honest label) / L (actual 1-Wire reader)

### 4. ESP-NOW receive callback writes `s_last` on the WiFi task while the UI task reads it
**File/function:** `src/espnow_mesh.cpp:handle` (`strncpy(s_last, …)`, `s_rx++`), read via `espnow_last()` from `src/main.cpp:1367` (`tool_espnow`)
`on_recv` runs in the WiFi/ESP-NOW task context; `handle` copies into the 64-byte static `s_last` with no lock or sequence guard, and `tool_espnow` hands `espnow_last()`'s pointer straight to `make_label` on the UI task. A message arriving mid-render can yield a torn string (half old, half new text), and `s_rx` is a non-volatile counter shared the same way. Fix: double-buffer with a sequence counter (or a small critical section) and have `espnow_last` copy into a caller buffer.
**Effort:** S

### 5. Config clamping lives inline in `setup()`, so the invariants the PIN/timer fixes rely on have no host test; `theme` is a dead field
**File/function:** `src/main.cpp:2744-2757` (`setup()` restore block), `src/config.h` (`DeviceCfg`, `cfg_serialize`), `test/test_config.cpp:43`
The brightness 10–100, dim 5–120, sleep 10–300, `sleep > dim`, and pin −1/0–9999 rules are re-implemented in `setup()` after `cfg_parse`, and the test file explicitly documents that `cfg_parse` does no clamping — so none of these rules is exercised under the ASan/UBSan host suite. Separately, `theme` is serialized as a literal `0` in `save_config_now` (`main.cpp:402`), ignored on load, and the Settings row says "only theme". Fix: add `cfg_clamp(DeviceCfg*)` to `config.h` (reusing `pwr_clamp`), call it from `setup()`, add tests for hostile values (`bright=-5;dim=9999;sleep=1;pin=12345`), and drop or implement `theme`.
**Effort:** S

## 2026-10-18

### 1. `tool_transit` never uses `transit.h`'s own balance helpers
**File/function:** `src/main.cpp:2234` (`tool_transit`) vs. `src/transit.h` (`le_u32`, `fmt_rupees`)
`transit.h` exists specifically to decode a card's balance block (`le_u32` pulls the little-endian paise field, `fmt_rupees` formats it as "Rs 245.50") and both are exercised by `test/test_transit.cpp`. But `tool_transit` in `main.cpp` only ever calls `nfc_read_uid()` and shows "card present - reading balance..." — it never reads a balance block and never calls either helper, so the screen can't actually show a balance even in demo form, unlike every other NFC/Sub-GHz tool that at least seeds `g_sub_*`/demo fields with a value its format function can render. Fix: add a demo/SD-fallback balance (same pattern `tool_subghz_capture` uses for `g_sub_code`) so the screen's own helpers get exercised and the UI shows more than a UID.
**Effort:** S

### 2. ISM decoder and wM-Bus meter never check for a live CC1101, unlike every other Sub-GHz tool
**File/function:** `src/main.cpp:982` (`tool_ism`) and `src/main.cpp:998` (`tool_wmbus`)
Every other Sub-GHz screen (`tool_freq_finder`, `tool_subghz_capture`, `tool_df`) branches on `cc1101_present()` to show live data when the radio answers and a clearly-labelled demo otherwise. `tool_ism` and `tool_wmbus` never call `cc1101_present()` at all — they unconditionally decode the same hardcoded `code`/`hdr[]` byte arrays on every render, so there is no path by which a real captured PT2262 gate code or wM-Bus frame could ever reach `pt2262_tristate`/`wmbus_manuf`/`wmbus_medium`. These two are the only Sub-GHz tools with zero live branch. Fix: gate on `cc1101_present()` and feed captured bytes (reusing the `subghz_capture_poll`/CC1101 async-RX path already built for the Capture & replay screen) when available.
**Effort:** M

### 3. Battery gauge is probed once at boot with no retry, unlike the NFC/NRF24 "I2C race" fixes already applied
**File/function:** `src/battery.cpp:batt_begin`, called once from `src/main.cpp:2807` (`setup()`); read via `batt_present()`/`batt_pct()` at `src/main.cpp:637` (`build_statusbar`)
`nfc_begin()` already has a documented retry path (`nfc_retry_action`/`nfc_retry_poll_cb` in `main.cpp`, added specifically because "if the PN532 handshake lost the race at boot, this screen was stuck... until reboot"), and NRF24 absence latching at first touch is already a tracked backlog item (2026-10-09 #4). `batt_begin()` has the identical shared-I2C-bus boot-race exposure (`gauge.begin(&Wire)` right after Wire is started) but is called exactly once in `setup()` with no retry anywhere — `build_statusbar` just calls `batt_present()` every redraw, so if the MAX17048 doesn't answer during that first narrow window, the battery icon never appears for the rest of the session, with no retry call or ACTION binding to recover it. Fix: same shape as `nfc_retry_poll_cb` — retry `batt_begin()` on a slow timer (or from `pm_tick`) until it succeeds.
**Effort:** S

### 4. "See through wall" always renders the same fixed 8-sample window — it never changes with time or input
**File/function:** `src/main.cpp:1221` (`tool_csi`) vs. `src/csi_motion.h` (`csi_variance`, `csi_motion`)
`tool_csi` builds the identical `float win[8] = {40,60,42,58,39,61,41,59}` literal every single time the screen is entered or redrawn, runs it through the host-tested `csi_variance`/`csi_motion`, and always prints the same "MOTION DETECTED" + "CSI variance 155" — there is no `esp_wifi` CSI callback, no timer, no randomization, nothing that varies the sample. Every other "bring-up demo" screen in the file at least seeds its one-shot demo state once and calls it a static reading; this one is presented as if it's live sensing ("ambient WiFi channel-state sensing") with a result that can never change no matter how long the screen stays open. Fix: at minimum perturb the demo window from `millis()` or a free-running counter so repeated visits don't show byte-identical "live" output, pending the real CSI callback.
**Effort:** S

### 5. GATT explore only ever resolves 4 of its own lookup table's 11 known services and never attempts a real connection
**File/function:** `src/main.cpp:2277` (`tool_gatt`) vs. `src/gatt_uuid.h` (`gatt_service_name`)
`gatt_service_name` maps 11 well-known 16-bit service UUIDs to names, but `tool_gatt`'s `static const uint16_t demo[] = {0x1800, 0x180A, 0x180F, 0x180D}` only ever looks up 4 of them, hardcoded, on every render — there is no BLEClient connect, no service discovery, and no path for a selected/scanned device (from `tool_ble_scan` or `tool_wof`, which already run real `ble_scan_async`) to be fed into this screen at all. Unlike `tool_ble_scan`, which at least performs a real scan, this tool performs no BLE operation whatsoever despite its name promising "explore". Fix: accept a device from the BLE scan list (same navigation pattern `nfc_crack_nav_action` uses to hand off between NFC screens) and wire a real `BLEClient::connect()` + `getServices()` walk through `gatt_service_name`.
**Effort:** M

## 2026-10-20

### 1. IR Learn & blast saves a format its own loader can never parse back
**File/function:** `src/main.cpp:1636` (`ir_save_action`) vs. `src/flipper_ir.h:56` (`flipper_ir_parse`) / `src/flipper_ir.h:94` (`flipper_ir_at`)
`ir_save_action` writes the learned code to `/ir/*.ir` as `"protocol:%u\nbits:%u\nvalue:0x%08llX\n"` — three numeric-keyed lines. But the *only* reader of `/ir/*.ir` files anywhere in the firmware is `ir_blast_action` (`src/main.cpp:1595`), which calls `flipper_ir_at`/`flipper_ir_parse`, and that parser requires a Flipper-format record with `name:`, `type:` (`parsed`/`raw`), `protocol:` as a *string* name like `NEC`, plus `address:`/`command:` byte fields — it explicitly returns false unless `fp->name[0]` and `fp->type != IRREC_NONE` are both set, neither of which `ir_save_action`'s output ever provides. So every code a user learns and saves via "Learn & blast" lands in the SD IRDB folder, gets counted by `storage_count_files("/ir", ".ir")` and shown in the Universal Remote's "+ SD IRDB: N .ir files" line, but silently fails to parse (and therefore never blasts) the moment it's selected there — the save path and the only read path speak two different, incompatible formats. Fix: have `ir_save_action` emit the same Flipper key format `flipper_ir_parse` expects (`name:`/`type: parsed`/`protocol:`/`address:`/`command:`), encoding `g_irl_proto`/`g_irl_value` the way `ir_send`'s own protocol already understands, so a learned code round-trips through the same IRDB the Universal Remote reads.
**Effort:** S

### 2. Mousejack's actual radio-injection function is never called — the tool only builds frames and stops
**File/function:** `src/main.cpp:1336` (`tool_mousejack`) vs. `src/nrf24_radio.cpp:80` (`nrf_mousejack_inject`)
`nrf_mousejack_inject(addr, channel, frames, n)` in `nrf24_radio.cpp` is a complete, already-tested (`mousejack_stream` is covered by `test/test_mousejack.cpp`) implementation that opens a writing pipe at a sniffed dongle address and transmits the HID frames with proper ESB pacing (`delayMicroseconds(1200)` between reports) — but a repo-wide search shows it has zero callers. `tool_mousejack` builds the frame stream with `mousejack_stream(0x00, payload, frames, 48)` and shows "payload -> N HID frames", then stops; it never has a sniffed address/channel to inject with and never calls `nrf_mousejack_inject` at all, unlike e.g. `tool_subghz_capture`'s capture→replay hookup, which is the shape this screen is missing. Right now the single wired-up NRF24 tool (`tool_mousejack`) can never actually inject a keystroke on real hardware no matter how it's used. Fix: wire a sniff step (snoop ESB traffic on `nrf_scan`'s busiest channel for a Logitech/Microsoft dongle address) ahead of the existing frame-build, then call `nrf_mousejack_inject` with the result on click, the same bring-up shape `wifi_deauth_tx`/`subghz_replay_action` already use elsewhere.
**Effort:** M
