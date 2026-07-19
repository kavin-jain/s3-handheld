// Pure SPI-flash JEDEC-ID decode for the firmware-dump tool — host-testable.
// Reading a SPI flash starts with the 0x9F JEDEC ID: manufacturer, memory type,
// capacity. This file names the vendor and turns the capacity code into a size.
// The SPI read-out + SD save is bring-up.
#pragma once
#include <stdint.h>

// Capacity byte is log2(size in bytes): 0x18 -> 16 MiB. 0 if out of range.
static inline uint32_t jedec_capacity_bytes(uint8_t cap) {
  return (cap >= 8 && cap <= 30) ? (1u << cap) : 0;
}

static inline const char *jedec_manuf(uint8_t m) {
  switch (m) {
    case 0xEF: return "Winbond";
    case 0xC2: return "Macronix";
    case 0x20: return "Micron";
    case 0x9D: return "ISSI";
    case 0x1F: return "Adesto";
    case 0xBF: return "SST";
    case 0x01: return "Spansion";
    case 0xC8: return "GigaDevice";
    default:   return "unknown";
  }
}
