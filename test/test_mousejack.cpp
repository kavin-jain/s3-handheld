// Host unit test for the pure Mousejack / Unifying frame codec.
//   g++ -std=c++17 test/test_mousejack.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/mousejack.h"
#include <cassert>

// The defining invariant: the whole packet sums to zero (mod 256).
static bool sums_to_zero(const uint8_t *p, int len) {
  uint8_t s = 0;
  for (int i = 0; i < len; i++) s = (uint8_t)(s + p[i]);
  return s == 0;
}

int main() {
  uint8_t f[UNIFYING_KBD_LEN];

  // Key 'a' (HID 0x04), no modifiers.
  mousejack_key(0x00, 0x00, 0x04, f);
  assert(f[1] == 0xC1 && f[3] == 0x04);
  assert(sums_to_zero(f, UNIFYING_KBD_LEN));
  // Payload sum before checksum = C1 + 04 = C5 -> checksum 0x3B.
  assert(f[9] == 0x3B);

  // Key release (all zero keys) is a valid frame too.
  mousejack_key(0x00, 0x00, 0x00, f);
  assert(sums_to_zero(f, UNIFYING_KBD_LEN));
  assert(f[9] == 0x3F);          // -(0xC1) & 0xff

  // With a modifier (Ctrl=0x01) + device index, invariant still holds.
  mousejack_key(0x02, 0x01, 0x06, f);   // Ctrl+'c'
  assert(sums_to_zero(f, UNIFYING_KBD_LEN));

  // Full injection stream: press+release per char.
  uint8_t stream[16][UNIFYING_KBD_LEN];
  int cnt = mousejack_stream(0x00, "hi", stream, 16);
  assert(cnt == 4);                                  // 2 chars * (press+release)
  // 'h' = HID 0x0B: press then release.
  assert(stream[0][2] == 0x00 && stream[0][3] == 0x0B);   // press h, no shift
  assert(stream[1][3] == 0x00);                            // release
  assert(stream[2][3] == 0x0C);                            // press i
  // Every frame in the stream is a valid Unifying frame (checksum).
  for (int i = 0; i < cnt; i++) assert(sums_to_zero(stream[i], UNIFYING_KBD_LEN));

  // Shifted char sets the Left-Shift modifier on the press frame.
  cnt = mousejack_stream(0x00, "A", stream, 16);
  assert(cnt == 2 && stream[0][2] == 0x02 && stream[0][3] == 0x04);

  // max_frames is respected (never overruns the buffer).
  cnt = mousejack_stream(0x00, "abcdef", stream, 4);
  assert(cnt <= 4);
  return 0;
}
