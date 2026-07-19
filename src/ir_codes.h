// Pure Flipper-.ir parsing helpers — no Arduino, so it host-unit-tests.
// A Flipper "parsed" IR record looks like:
//   name: Power
//   protocol: NEC
//   address: 04 00 00 00
//   command: 08 00 00 00
#pragma once
#include <stddef.h>

static inline int ir_hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// If line is "key: value", copy the trimmed value into out and return true.
static inline bool ir_kv(const char *line, const char *key, char *out, size_t cap) {
  if (!line || !key || !out || cap == 0) return false;
  out[0] = 0;
  size_t i = 0;
  while (line[i] == ' ' || line[i] == '\t') i++;
  size_t j = 0;
  while (key[j]) { if (line[i + j] != key[j]) return false; j++; }
  if (line[i + j] != ':') return false;
  const char *v = line + i + j + 1;
  while (*v == ' ' || *v == '\t') v++;
  size_t o = 0;
  while (*v && *v != '\r' && *v != '\n' && o < cap - 1) out[o++] = *v++;
  while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '\t')) o--;   // trim trailing ws
  out[o] = 0;
  return true;
}

// First hex byte of a Flipper byte field "04 00 00 00" -> 0x04. -1 on failure.
static inline int ir_first_byte(const char *s) {
  if (!s) return -1;
  while (*s == ' ' || *s == '\t') s++;
  int hi = ir_hexval(s[0]);
  int lo = hi < 0 ? -1 : ir_hexval(s[1]);
  if (hi < 0 || lo < 0) return -1;
  return (hi << 4) | lo;
}
