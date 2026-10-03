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
- ☐ Settings screen: Theme/Storage/About rows no longer push a no-op nav_stack frame on
      click (were wired to nav_push(SCR_SETTINGS,...), i.e. "navigate" to the screen
      already on screen -- invisible, but it silently consumed a BACK press per tap).
      They're NAV_NONE now. Confirm BACK from Settings always returns home in one press
      regardless of which rows were tapped first.

## Power
- ☐ Backlight LED rewired off 3V3 → GPIO46 via MOSFET (else dimming is a no-op).
- ☐ Idle → dims at ~20 s, screen off + light-sleep at ~35 s, any input wakes instantly.
      Both now Settings > Dim timer / Sleep timer (SCR_EDIT_DIM/SCR_EDIT_SLEEP, g_dim_s/
      g_sleep_s) instead of the DIM_AFTER_MS/SLEEP_AFTER_MS #defines -- those two screens
      previously didn't exist; "Sleep timers" in Settings showed the compile-time values
      but rotating/clicking it did nothing (nav_push'd to the Settings screen already on
      screen). Confirm rotating each actually changes when dim/sleep kick in on real
      hardware, and that sleep_s is kept 5s+ above dim_s (enforced in dim_edit_cb/
      sleep_edit_cb, main.cpp) rather than letting pm_tick's two sequential idle checks
      fire the same tick.
- ☐ Measure real current (USB meter) in active / dim / sleep; log mA for the 10 h math.

## Sub-GHz (2×CC1101, SPI-B)
- ☐ CC1101 #1 version-register reads (0x14/0x04) — proves SPI-B works before stacking radios.
- ☐ Frequency finder locks onto a known 433.92 remote; MHz + RSSI sane. click = lock now
      writes the tuned band into g_sub_mhz (main.cpp) -- previously the finder's tuning
      was cosmetic: Capture&replay and the Direction finder each hardcoded 433.92f
      independently, so nothing you found here ever changed what they listened/sent on.
      Confirm RSSI at a non-433 band (e.g. 915) still reads sane after a lock.
- ☐ Fixed-code capture then replay opens your own gate. Rolling code shows "can't copy".
      click = replay is now wired (subghz_replay_action) -- confirm a captured code
      actually re-opens the gate, and that the locked g_sub_mhz (not always 433.92) is
      what's transmitted on. Demo mode (no CC1101) now seeds real demo values instead of
      stale/zeroed globals, so ACTION-save and click-replay act on what's shown on screen.

## NFC (PN532, I2C)
- ☐ Reads a Mifare Classic UID; dictionary attack recovers a known key. click = crack keys
      on the Read/clone screen now navigates to Mifare crack (T_NFC[1]) -- that screen is
      still its own standalone bring-up demo and doesn't yet take the just-read UID as
      input; clicking previously did nothing at all.
- ☐ Reads a transit card balance; EMV reads PAN/expiry only.
- ☐ Writes an NDEF tag that a phone opens.

## IR (TX GPIO47 / RX GPIO48)
- ☐ Learns a real remote; blasts it back and the TV responds.
- ☐ Universal-remote brand DB powers off a known TV/AC. click = blast on a TV
      brand now actually calls ir_send() (was previously advertised in the
      screen's own help text but never wired to anything -- clicking did
      nothing). A/C brands still don't send anything on click: a real A/C
      blast needs IRremoteESP8266's IRac class, not implemented in this
      firmware yet -- ACTION's temp-bump display is the only A/C interaction
      that currently does anything.
- ☐ IRDB from SD: drop CC0 Flipper-IRDB .ir files under /ir; the universal
      remote now lists them after the TV/AC brands and click = blast sends
      each file's first parsed record via flipper_ir_at + ir_send_flipper
      (NEC/NECext/Samsung32/SIRC*/RC6). Verify addr/cmd byte order matches the
      real remote (raw + RC5/Kaseikyo still unmapped); a file's 2nd+ buttons
      aren't reachable from this UI yet (first record only).
- ☐ The encoder-click dispatch itself (g_click_cb in main.cpp's poll_buttons) is new
      wiring with no hardware behind it yet -- confirm a real encoder click actually
      fires it (vs. e.g. bouncing/double-firing against the existing button-tick buzzer
      feedback). Now wired on 8 screens total, not just IR: Frequency finder (lock),
      Capture&replay (replay), Direction finder (resample), NFC Read/clone (crack-keys
      nav), WiFi scan (rescan, both the found-networks and no-networks-found branches),
      BLE scan (rescan, both branches), NRF24 band scanner (rescan). Every one of these
      previously had "click = X" in its own help text with nothing behind it --
      poll_buttons simply never dispatched the encoder-click edge to anything before this
      session. Confirm each one on real hardware, not just the one IR click tested so far.

## WiFi / BLE (native)
- ☐ Scan lists real APs + clients.
- ☐ Deauth drops a client off *your own* AP.
- ☐ Evil Portal serves the page; captured creds land on SD.
- ☐ BLE scan sees phones/trackers; tracker-hunt flags a planted AirTag.

## NRF24 (2×NRF24, SPI-B) — add 10 µF caps first
- ☐ Band scanner shows 2.4 GHz activity. Rotary range fixed to 0..39 (NRF_CHAN-1) --
      was 0..125, the full real nRF24 channel space, but nrf_scan() only ever sweeps
      the lower 40 channels, so dialling past 39 showed a plausible MHz value with a
      "<busiest>" flag that was silently always false (no data there to begin with).
- ☐ click = rescan wired (nrf_rescan_action); nrf_scan_request() now also clears the
      cached s_scanned flag so a poll timer started right after doesn't see "done"
      on its first tick and repaint the stale pre-rescan counts. Confirm a rescan on
      real hardware actually shows changed activity, not a flash of old data.
- ☐ Band scanner's sweep now runs on a dedicated core-0 task (radio_task.h/.cpp)
      instead of inline on the UI task — confirm the UI (encoder, animation) stays
      responsive during the ~104ms sweep, and that spi_b_mutex genuinely serializes
      the radio task against any other SPI-B access under real preemption (this
      repo's prior single-task cooperative access never exercised that).
- ☐ Mousejack: sniff a dongle address, nrf_mousejack_inject a mousejack_stream payload,
      verify keystrokes land in a text editor on the paired host (own gear only).
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
- ☐ Me → Calendar/Tasks: drop /me/calendar.txt ("<iCal-dt> <title>" lines) and /me/tasks.txt
      ("[ ] !N text") on SD; screens render them (ical_next_event / task_parse), demo if absent.

## BadUSB / Pranks (USB-OTG HID)
- ☐ USB gag: pick a gag with the encoder, plug into a host, press ACTION — badusb_run_line
      types the DuckyScript line (Rickroll URL / GUI r / Lock screen). Own machines only.
- ☐ DuckyScript: drop a payload .txt under /ducky, rotate to pick it, ACTION runs
      it (storage_nth_file + badusb_run_line, wired this pass -- previously the
      screen advertised this but nothing in the firmware actually opened an SD
      file). Confirm keystrokes land on the host and multi-line payloads
      (STRING/DELAY/ENTER/GUI sequences) run in order, not just a single line.

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
- ☐ Settings persist: change brightness, reboot, value restored from /config.txt (config.h
      round-trip). Now also dim/sleep timers -- config.h always serialized/parsed dim=/
      sleep=, but main.cpp discarded the parsed value and re-wrote the DIM_AFTER_MS/
      SLEEP_AFTER_MS #defines on every save; a changed timer silently reverted on reboot.
      g_dim_s/g_sleep_s are the real runtime values now. Confirm both round-trip too.
- ☐ Sub-GHz capture: press ACTION, a .sub file appears in /subghz and the path shows on screen.
