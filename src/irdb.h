// Pure IR brand power-code database for the universal remote — host-testable.
// Stores well-documented 32-bit TV codes per brand; the actual blast goes out
// through ir_send() (IRremoteESP8266) and is bring-up. Protocol ids match
// IRremoteESP8266's decode_type_t so the sender can pick the right encoder.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum { IRP_NEC = 3, IRP_SAMSUNG = 7 };

struct IrBrand {
  const char *name;
  uint8_t  proto;
  uint32_t power, vol_up, vol_dn;
  uint16_t bits;
};

// Widely-published TV codes. On-device they still need confirming against the
// actual set (see docs/BRINGUP.md) — IR codes vary by model/region.
static const IrBrand IR_BRANDS[] = {
  {"Samsung", IRP_SAMSUNG, 0xE0E040BF, 0xE0E0E01F, 0xE0E0D02F, 32},
  {"LG",      IRP_NEC,     0x20DF10EF, 0x20DF40BF, 0x20DFC03F, 32},
};
static const int IR_BRAND_COUNT = sizeof(IR_BRANDS) / sizeof(IR_BRANDS[0]);

static inline int ir_brand_count(void) { return IR_BRAND_COUNT; }

static inline const IrBrand *ir_brand_at(int i) {
  return (i >= 0 && i < IR_BRAND_COUNT) ? &IR_BRANDS[i] : NULL;
}

static inline const IrBrand *ir_find_brand(const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < IR_BRAND_COUNT; i++) {
    const char *a = IR_BRANDS[i].name, *b = name;
    while (*a && *b && *a == *b) { a++; b++; }
    if (!*a && !*b) return &IR_BRANDS[i];
  }
  return NULL;
}
