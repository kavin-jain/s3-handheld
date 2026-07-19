// Pure PT2262 / EV1527 tri-state decode for the 433 ISM decoder — host-testable.
// PT2262 garage/gate remotes encode each pin as a tri-state symbol (0 / 1 / F
// "floating"). Over the air that's two OOK bits per symbol, so an RCSwitch-
// decoded binary code maps back to tri-state: 00->'0', 11->'1', 01->'F'.
// The RF capture (subghz_replay.h / CC1101) is bring-up; this is the mapping.
#pragma once
#include <stdint.h>

// Decode `nbits` (even) of `code`, MSB-first, into nbits/2 tri-state chars.
// out needs nbits/2 + 1. Unrecognised pairs (10) become '?'. Returns symbol count.
static inline int pt2262_tristate(uint32_t code, int nbits, char *out) {
  int sym = nbits / 2;
  for (int i = 0; i < sym; i++) {
    int shift = (sym - 1 - i) * 2;
    int pair = (code >> shift) & 0x3;
    out[i] = pair == 0 ? '0' : pair == 3 ? '1' : pair == 1 ? 'F' : '?';
  }
  out[sym] = 0;
  return sym;
}

// A clean PT2262 frame has no unrecognised (10) pairs.
static inline bool pt2262_is_valid(const char *tri) {
  for (const char *p = tri; *p; p++)
    if (*p == '?') return false;
  return true;
}
