// Pure transit-card value helpers — host-testable. The balance block/offset and
// auth key are card-specific (Delhi Metro etc.) and a bring-up detail; these are
// the reusable primitives every parser needs.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Little-endian uint32 from a byte block at offset.
static inline uint32_t le_u32(const uint8_t *b, int off) {
  return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) |
         ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
}

// Paise -> "Rs 245.50". (Rupee glyph avoided — the terminal font is ASCII.)
static inline size_t fmt_rupees(uint32_t paise, char *out, size_t cap) {
  int n = snprintf(out, cap, "Rs %u.%02u", paise / 100, paise % 100);
  if (n < 0 || (size_t)n >= cap) { if (cap) out[0] = 0; return 0; }
  return (size_t)n;
}
