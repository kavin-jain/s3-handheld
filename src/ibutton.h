// Pure Dallas/Maxim 1-Wire CRC8 + iButton ROM validation — host-testable.
// The 1-Wire GPIO read itself is bring-up (bit-banged protocol).
#pragma once
#include <stdint.h>

static inline uint8_t onewire_crc8(const uint8_t *data, int len) {
  uint8_t crc = 0;
  for (int i = 0; i < len; i++) {
    uint8_t b = data[i];
    for (int j = 0; j < 8; j++) {
      uint8_t mix = (crc ^ b) & 1;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      b >>= 1;
    }
  }
  return crc;
}

// An 8-byte iButton ROM is valid when byte 7 == CRC8 of bytes 0..6.
static inline bool ibutton_valid(const uint8_t rom[8]) {
  return rom && onewire_crc8(rom, 7) == rom[7];
}
