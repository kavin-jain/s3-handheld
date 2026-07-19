// Pure Wireless M-Bus (EN 13757) header decode — host-testable.
// Utility meters (water/gas/heat/electricity) beacon on 868.95 MHz in mode T/C.
// The RF capture + 3-of-6 / Manchester chip decode is bring-up (CC1101); this
// file decodes the link-layer header fields and validates the block CRC.
#pragma once
#include <stdint.h>
#include <stdio.h>

// EN 61107 / FLAG 16-bit manufacturer code -> 3 uppercase letters (e.g. "ELS").
// Each letter is 5 bits, value = letter - 64 ('A'=1).
static inline void wmbus_manuf(uint16_t id, char out[4]) {
  out[0] = (char)(((id >> 10) & 0x1F) + 64);
  out[1] = (char)(((id >> 5) & 0x1F) + 64);
  out[2] = (char)((id & 0x1F) + 64);
  out[3] = 0;
}

// Inverse — build the 16-bit code from a 3-letter flag (round-trip / filtering).
static inline uint16_t wmbus_manuf_id(const char *s) {
  return (uint16_t)((((s[0] - 64) & 0x1F) << 10) |
                    (((s[1] - 64) & 0x1F) << 5) |
                    ((s[2] - 64) & 0x1F));
}

// EN 13757-3 device-type / medium byte -> name (common subset).
static inline const char *wmbus_medium(uint8_t m) {
  switch (m) {
    case 0x00: return "Other";
    case 0x02: return "Electricity";
    case 0x03: return "Gas";
    case 0x04: return "Heat";
    case 0x06: return "Warm Water";
    case 0x07: return "Water";
    case 0x08: return "Heat Cost Alloc";
    case 0x0A: return "Cooling";
    case 0x16: return "Cold Water";
    case 0x18: return "Waste Water";
    default:   return "Unknown";
  }
}

// EN 13757 wM-Bus block CRC-16: poly 0x3D65, init 0x0000, final XOR 0xFFFF,
// MSB-first, no reflection. Each on-air block carries this over its data bytes.
static inline uint16_t wmbus_crc(const uint8_t *d, int n) {
  uint16_t crc = 0x0000;
  for (int i = 0; i < n; i++) {
    crc ^= (uint16_t)d[i] << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x3D65) : (uint16_t)(crc << 1);
  }
  return (uint16_t)(crc ^ 0xFFFF);
}
