// Pure path logic for the Flipper-style "save everything" layer.
// NO Arduino/SD includes here on purpose — so it compiles + unit-tests on the
// host (see test/test_storage_paths.cpp). The hardware side lives in storage.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum SaveKind { SAVE_SUBGHZ = 0, SAVE_NFC, SAVE_IR, SAVE_WIFI, SAVE_BADUSB, SAVE_KIND_N };

static inline const char *sp_dir(int k) {
  switch (k) {
    case SAVE_SUBGHZ: return "/subghz";
    case SAVE_NFC:    return "/nfc";
    case SAVE_IR:     return "/ir";
    case SAVE_WIFI:   return "/wifi";
    case SAVE_BADUSB: return "/badusb";
    default:          return "/misc";
  }
}

static inline const char *sp_prefix(int k) {
  switch (k) {
    case SAVE_SUBGHZ: return "sub";
    case SAVE_NFC:    return "nfc";
    case SAVE_IR:     return "ir";
    case SAVE_WIFI:   return "wifi";
    case SAVE_BADUSB: return "duck";
    default:          return "cap";
  }
}

// Build "/nfc/nfc_0007.nfc" into out. Returns chars written (excl NUL), or 0 on
// bad args / overflow (and NUL-terminates out when it can).
// ponytail: seq wraps at 10000 per kind — plenty; bump the width if you ever hoard more.
static inline size_t sp_make_path(char *out, size_t cap, int k, const char *ext, uint32_t seq) {
  if (!out || cap == 0) return 0;
  out[0] = 0;
  if (k < 0 || k >= SAVE_KIND_N || !ext) return 0;
  int n = snprintf(out, cap, "%s/%s_%04lu.%s", sp_dir(k), sp_prefix(k),
                   (unsigned long)(seq % 10000), ext);
  if (n < 0 || (size_t)n >= cap) { out[0] = 0; return 0; }
  return (size_t)n;
}
