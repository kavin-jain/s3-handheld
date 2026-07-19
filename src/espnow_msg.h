// Pure ESP-NOW message framing — host-testable. Frame = [MAGIC][len][payload][xor].
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ESPNOW_MAGIC 0xE5

// Build a frame from text. Returns total bytes, or 0 on overflow/bad args.
static inline size_t espnow_frame(const char *text, uint8_t *out, size_t cap) {
  if (!text || !out) return 0;
  size_t len = strlen(text);
  if (len > 250 || len + 3 > cap) return 0;
  out[0] = ESPNOW_MAGIC;
  out[1] = (uint8_t)len;
  uint8_t x = 0;
  for (size_t i = 0; i < len; i++) { out[2 + i] = (uint8_t)text[i]; x ^= (uint8_t)text[i]; }
  out[2 + len] = x;
  return len + 3;
}

// Validate + extract text. Returns text length (out NUL-terminated) or 0 if invalid.
static inline size_t espnow_parse(const uint8_t *buf, size_t buflen, char *out, size_t cap) {
  if (!buf || !out || cap == 0 || buflen < 3) return 0;
  if (buf[0] != ESPNOW_MAGIC) return 0;
  size_t len = buf[1];
  if (buflen < len + 3 || len + 1 > cap) return 0;
  uint8_t x = 0;
  for (size_t i = 0; i < len; i++) x ^= buf[2 + i];
  if (x != buf[2 + len]) return 0;
  memcpy(out, buf + 2, len);
  out[len] = 0;
  return len;
}
