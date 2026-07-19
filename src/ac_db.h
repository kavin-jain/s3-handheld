// Pure air-conditioner brand database for the universal remote — host-testable.
// Maps a brand to its IRremoteESP8266 protocol (decode_type_t value). The actual
// send goes through IRac/IRsend (bring-up); these ids are verified against the
// installed library by static_asserts in ir_remote.cpp, so a lib bump that
// renumbers the enum fails the build instead of sending the wrong protocol.
#pragma once
#include <stddef.h>

struct AcBrand { const char *name; int proto; };  // proto = decode_type_t value

// 19 of the most common AC brands worldwide, each a full-state protocol IRac
// can drive (temp / mode / fan). Values from IRremoteESP8266.h decode_type_t.
static const AcBrand AC_BRANDS[] = {
  {"Coolix (generic)", 15}, {"Daikin",     16}, {"Kelvinator", 18},
  {"Mitsubishi",       20}, {"Gree",       24}, {"Toshiba",    32},
  {"Fujitsu",          33}, {"Midea",      34}, {"Carrier",    37},
  {"Haier",            38}, {"Hitachi",    40}, {"Whirlpool",  45},
  {"Samsung",          46}, {"Electra",    48}, {"Panasonic",  49},
  {"LG",               51}, {"TCL",        57}, {"Sanyo",      89},
  {"Voltas",           90},
};
static const int AC_BRAND_COUNT = sizeof(AC_BRANDS) / sizeof(AC_BRANDS[0]);

static inline const AcBrand *ac_brand_at(int i) {
  return (i >= 0 && i < AC_BRAND_COUNT) ? &AC_BRANDS[i] : NULL;
}

static inline const AcBrand *ac_find_brand(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < AC_BRAND_COUNT; i++) {
    const char *a = AC_BRANDS[i].name, *b = name;
    while (*a && *b && *a == *b) { a++; b++; }
    if (!*a && !*b) return &AC_BRANDS[i];
  }
  return NULL;
}
