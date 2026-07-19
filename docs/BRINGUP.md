# On-device bring-up checklist

A feature is **working** only when its box here is checked on real hardware. Firmware that
compiles and passes logic tests is *ready to bring up* — not proven. Check the fast/shared
things first; a dead I2C bus or SPI bus fails everything downstream.

Legend: ☐ not verified · ☑ verified on hardware · ⚠ issue (note it)

## Phase 0 — board alive
- ☐ Flashes and boots over native USB-CDC; serial prints `[ui] home ready`.
- ☐ TFT renders the black/green home screen, no tearing.
- ☐ Encoder rotates the selection smoothly (tune `ENC_STEPS_PER_DETENT` if double/half steps).
- ☐ I2C scan finds MCP23017 @0x20, GY-87/MPU6050 @0x68, PN532 @0x24.
- ☐ BACK / HOME / ACTION buttons + encoder click navigate.
- ☐ Vibration motor + buzzer fire (MCP GPA6 / GPA5).

## Power
- ☐ Backlight LED rewired off 3V3 → GPIO46 via MOSFET (else dimming is a no-op).
- ☐ Idle → dims at ~20 s, screen off + light-sleep at ~35 s, any input wakes instantly.
- ☐ Measure real current (USB meter) in active / dim / sleep; log mA for the 10 h math.

## Sub-GHz (2×CC1101, SPI-B)
- ☐ CC1101 #1 version-register reads (0x14/0x04) — proves SPI-B works before stacking radios.
- ☐ Frequency finder locks onto a known 433.92 remote; MHz + RSSI sane.
- ☐ Fixed-code capture then replay opens your own gate. Rolling code shows "can't copy".

## NFC (PN532, I2C)
- ☐ Reads a Mifare Classic UID; dictionary attack recovers a known key.
- ☐ Reads a transit card balance; EMV reads PAN/expiry only.
- ☐ Writes an NDEF tag that a phone opens.

## IR (TX GPIO47 / RX GPIO48)
- ☐ Learns a real remote; blasts it back and the TV responds.
- ☐ Universal-remote brand DB powers off a known TV/AC.

## WiFi / BLE (native)
- ☐ Scan lists real APs + clients.
- ☐ Deauth drops a client off *your own* AP.
- ☐ Evil Portal serves the page; captured creds land on SD.
- ☐ BLE scan sees phones/trackers; tracker-hunt flags a planted AirTag.

## NRF24 (2×NRF24, SPI-B) — add 10 µF caps first
- ☐ Band scanner shows 2.4 GHz activity.
- ☐ Mousejack detects a vulnerable dongle.

## Storage (onboard SD, SPI-A, CS GPIO9)
- ☐ SD mounts; captures write to /subghz /nfc /ir /wifi and reload.
