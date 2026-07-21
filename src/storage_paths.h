// Pure path logic for the Flipper-style "save everything" layer.
// NO Arduino/SD includes here on purpose — so it compiles + unit-tests on the
// host (see test/test_storage_paths.cpp). The hardware side lives in storage.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum SaveKind { SAVE_SUBGHZ = 0, SAVE_NFC, SAVE_IR, SAVE_WIFI, SAVE_BADUSB, SAVE_FW, SAVE_KIND_N };

static inline const char *sp_dir(int k) {
  switch (k) {
    case SAVE_SUBGHZ: return "/subghz";
    case SAVE_NFC:    return "/nfc";
    case SAVE_IR:     return "/ir";
    case SAVE_WIFI:   return "/wifi";
    case SAVE_BADUSB: return "/badusb";
    case SAVE_FW:     return "/fw";
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
    case SAVE_FW:     return "dump";
    default:          return "cap";
  }
}

// Extract the sequence number from a bare filename ("nfc_0007.nfc") for kind k.
// Returns the number, or -1 if the name isn't "<prefix>_<digits>.<ext>" for k.
// Lets the hardware scan a directory ONCE for the max seq instead of probing
// every candidate path — O(entries) rather than O(10000) stat calls per save.
static inline int sp_parse_seq(const char *fname, int k) {
  if (!fname || k < 0 || k >= SAVE_KIND_N) return -1;
  const char *pfx = sp_prefix(k);
  size_t i = 0;
  for (; pfx[i]; i++) if (fname[i] != pfx[i]) return -1;   // prefix must match
  if (fname[i] != '_') return -1;
  const char *d = fname + i + 1;
  if (d[0] < '0' || d[0] > '9') return -1;                 // need >=1 digit
  int val = 0, c = 0;
  while (d[c] >= '0' && d[c] <= '9') { val = val * 10 + (d[c] - '0'); c++; }
  if (d[c] != '.') return -1;                              // digits then extension
  return val;
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
