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
