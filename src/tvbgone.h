// TV-B-Gone — blast common TV "power" codes in sequence. Table is pure/testable;
// the blast (tvbgone_fire_all) lives in tvbgone.cpp over the IR driver.
// proto values mirror IRremoteESP8266 decode_type_t: RC5=1, NEC=3, SONY=4, SAMSUNG=7.
#pragma once
#include <stdint.h>
#include <stddef.h>

struct TvCode { uint8_t proto; uint64_t value; uint16_t bits; };

static const TvCode TVB_CODES[] = {
  {7, 0xE0E040BFULL, 32},   // Samsung power
  {3, 0x20DF10EFULL, 32},   // LG (NEC) power
  {4, 0xA90ULL,      12},   // Sony power (12-bit)
  {3, 0x807F02FDULL, 32},   // generic NEC TV power
  {1, 0x100CULL,     13},   // Philips RC5 power
};

static inline int tvb_count() { return (int)(sizeof(TVB_CODES) / sizeof(TVB_CODES[0])); }

static inline bool tvb_get(int i, TvCode *out) {
  if (i < 0 || i >= tvb_count() || !out) return false;
  *out = TVB_CODES[i];
  return true;
}

void tvbgone_fire_all(int gap_ms);   // hardware side (tvbgone.cpp)
