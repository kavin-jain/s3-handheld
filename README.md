# Handheld — multi-radio security tool (Edgehax S3-PRO)

Custom Flipper-class firmware for an **ESP32-S3-WROOM-1-N16R8** handheld: 2×CC1101 (sub-GHz),
2×NRF24 (2.4 GHz), PN532 (13.56 MHz NFC), IR TX/RX, native WiFi/BLE/USB-OTG, GPS, GY-87 IMU,
2.4" ILI9341 TFT, rotary encoder + 3 buttons via an MCP23017. Interpretation-first UI on LVGL:
the device says *what* a signal is in plain language, with the real MHz/hex one click away.

> Personal security-research device. Use only against hardware and networks you own or are
> authorized to test. No jammers, no rolling-code theft — see the capability matrix.

## Build & flash

```bash
pio run                                    # compile
pio run -t upload && pio device monitor    # flash + serial (115200)
```

PlatformIO + Arduino-ESP32. All driver libraries are pulled via `platformio.ini` `lib_deps`
(nothing is vendored). IR/NFC databases live in `assets/` (gitignored; curated onto the SD card
at build time).

## Architecture

| Area | Where | Notes |
|------|-------|-------|
| Pin map | `include/pins.h` | single source of truth, from the verified wiring contract |
| UI | `src/main.cpp` | LVGL shell: black/green theme, encoder + button nav, 13-category menu |
| LVGL config | `include/lv_conf.h` | 16-bit colour, UNSCII terminal font |
| Power | `pm_tick()` in `src/main.cpp` | idle → dim → light-sleep (GPIO wake) |

Two SPI buses (display fast on SPI-A, four radios slow on SPI-B), dual-core (UI on core 1,
radios on core 0). See the wiring contract for the full pin allocation.

## Feature status

The full curated capability matrix (61 buildable tools across 13 categories, plus the
documented-never-built "red zone") is tracked separately. Each feature moves through:

**compiles → pure logic unit-tested → on-device bring-up verified.** A feature is only marked
*working* after the last step, on real hardware against real targets — see `docs/BRINGUP.md`.

## Licensing

This firmware is **GPL-3.0** (it links GPL driver libraries such as RF24). See `LICENSE`.

Upstream projects (HIZMOS, Bruce/Marauder) are used **only as read-only reference** — their code
is **not** copied into this repo. HIZMOS ships without a license (all rights reserved) and Bruce
is AGPL-3.0; vendoring either would be a licensing problem. All implementations here are original,
written against the libraries listed in `platformio.ini`. See `NOTICE`.
