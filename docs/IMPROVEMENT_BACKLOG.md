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
