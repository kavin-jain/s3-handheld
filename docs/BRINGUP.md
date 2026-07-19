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
- ☐ IRDB from SD: drop CC0 Flipper-IRDB .ir files under /ir; flipper_ir.h parses a
      record and ir_send_flipper (NEC/NECext/Samsung32/SIRC*/RC6) blasts it. Verify
      addr/cmd byte order matches the real remote (raw + RC5/Kaseikyo still unmapped).

## WiFi / BLE (native)
- ☐ Scan lists real APs + clients.
- ☐ Deauth drops a client off *your own* AP.
- ☐ Evil Portal serves the page; captured creds land on SD.
- ☐ BLE scan sees phones/trackers; tracker-hunt flags a planted AirTag.

## NRF24 (2×NRF24, SPI-B) — add 10 µF caps first
- ☐ Band scanner shows 2.4 GHz activity.
- ☐ Mousejack detects a vulnerable dongle; injected 'a' frame moves a cursor.
- ☐ Keyboard sniff decodes real keystrokes from an unencrypted 2.4 GHz kbd.

## Sub-GHz — extras
- ☐ Capture&replay decodes an RCSwitch fob (protocol auto-detected) and re-sends it.
- ☐ wM-Bus: CC1101 @ 868.95 T/C-mode receives a meter frame; manuf/medium/CRC parse.

## NFC — extras
- ☐ Amiibo: PN532 dumps 540 B NTAG215; figure-id + BCC validate; write to blank NTAG215.
- ☐ iButton: 1-Wire read of a Dallas key; CRC8 validates.

## Counter-surveillance ("Am I safe?")
- ☐ Hidden-camera / audio-bug sweep flags a live transmitter in a covert band.
- ☐ Skimmer detector flags a planted HC-05 near a reader.
- ☐ Deauth detector counts frames during a test deauth.
- ☐ Drone spotter decodes a real OpenDroneID beacon (operator id + GPS).

## WiFi — extras
- ☐ Handshake: capture M1-M4 to a .pcap on SD; hashcat cracks a known PSK.
- ☐ Karma: phone auto-associates to a probed SSID. Wardrive appends WiGLE rows + GPS.

## Tools / Bench
- ☐ Bus Pirate I2C scan names the board's own chips (0x20/0x24/0x36/0x68).
- ☐ Firmware dump: JEDEC ID reads; full chip dumps to dump.bin on SD.
- ☐ GPIO play toggles a usable pin; refuses reserved 26-37.

## Comms / Me
- ☐ ESP-NOW mesh exchanges a message with a second unit.
- ☐ Phone BLE bridge feeds usage / calendar (iCal) / tasks to the device.

## Max-power config (green-zone attacks run the radios at their hardware limit)
- ☐ CC1101 setPA(12) = +12 dBm on the async TX (replay). Confirm range vs default.
- ☐ NRF24 RF24_PA_MAX + 2 Mbps + autoack/CRC off (promiscuous). Add a PA/LNA module for more.
- ☐ WiFi esp_wifi_set_max_tx_power(84) ≈ 20.5 dBm on scan + deauth.
- ⚠ REGULATORY: max chip power can exceed local ISM/WiFi ERP/EIRP limits. Legal to run
     only where you're licensed/permitted and on your own targets — operator's responsibility.

## Storage (onboard SD, SPI-A, CS GPIO9)
- ☐ SD mounts; captures write to /subghz /nfc /ir /wifi and reload.
- ☐ Save N captures of one kind: filenames increment (0000,0001,...) with no overwrite
      (next_seq scans the folder once via sp_parse_seq; verify e.name() is basename-or-path safe).
- ☐ Settings persist: change brightness, reboot, value restored from /config.txt (config.h round-trip).
- ☐ Sub-GHz capture: press ACTION, a .sub file appears in /subghz and the path shows on screen.
