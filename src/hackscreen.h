// Pure deterministic PRNG for the "hacker screen" prank — host-testable.
// Draws a Hollywood cascade of hex. A seeded xorshift keeps it reproducible
// (so it can be tested) while still looking random on screen.
#pragma once
#include <stdint.h>

// xorshift32 — advance and return the next state (never returns 0 if seeded !=0).
static inline uint32_t hack_next(uint32_t *s) {
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return *s = x;
}

static inline char hack_hex(uint32_t v) {
  v &= 0xF;
  return v < 10 ? (char)('0' + v) : (char)('a' + (v - 10));
}

// Fill out[0..n) with hex chars from the stream and NUL-terminate.
static inline void hack_line(uint32_t *s, char *out, int n) {
  for (int i = 0; i < n; i++) out[i] = hack_hex(hack_next(s));
  out[n] = 0;
}
