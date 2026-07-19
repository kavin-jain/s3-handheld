// Host unit test for the pure HID usage-code -> ASCII map.
//   g++ -std=c++17 test/test_hid_keymap.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/hid_keymap.h"
#include <cassert>
#include <cstring>

int main() {
  // Letters, case via shift.
  assert(hid_to_ascii(0x04, false) == 'a');
  assert(hid_to_ascii(0x04, true)  == 'A');
  assert(hid_to_ascii(0x1D, false) == 'z');
  // Numbers and their shifted symbols.
  assert(hid_to_ascii(0x1E, false) == '1');
  assert(hid_to_ascii(0x1E, true)  == '!');
  assert(hid_to_ascii(0x27, false) == '0');
  assert(hid_to_ascii(0x27, true)  == ')');
  // Punctuation + whitespace.
  assert(hid_to_ascii(0x2C, false) == ' ');
  assert(hid_to_ascii(0x28, false) == '\n');
  assert(hid_to_ascii(0x33, true)  == ':');
  assert(hid_to_ascii(0x00, false) == 0);         // unmapped

  // Modifier decode.
  assert(hid_shift(0x02) && hid_shift(0x20));
  assert(!hid_shift(0x01));                        // ctrl, not shift

  // Decode a whole "Hello" report stream (shifted H, then ello).
  struct { uint8_t mod, key; } rpt[] = {
    {0x02,0x0B},{0x00,0x08},{0x00,0x0F},{0x00,0x0F},{0x00,0x12}};
  char out[8]; int n = 0;
  for (auto &r : rpt) out[n++] = hid_to_ascii(r.key, hid_shift(r.mod));
  out[n] = 0;
  assert(strcmp(out, "Hello") == 0);
  return 0;
}
