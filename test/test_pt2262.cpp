// Host unit test for the pure PT2262 tri-state decode.
//   g++ -std=c++17 test/test_pt2262.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/pt2262.h"
#include <cassert>
#include <cstring>

int main() {
  char out[16];

  // 6 bits 00 11 01 -> "01F".
  int n = pt2262_tristate(0x0D, 6, out);   // 0b001101
  assert(n == 3 && strcmp(out, "01F") == 0);
  assert(pt2262_is_valid(out));

  // All-'0' and all-'1' frames.
  pt2262_tristate(0x000, 6, out);
  assert(strcmp(out, "000") == 0);
  pt2262_tristate(0x3F, 6, out);           // 0b111111 -> "111"
  assert(strcmp(out, "111") == 0);

  // A 10 pair is unrecognised -> '?', frame invalid.
  pt2262_tristate(0x2, 2, out);            // 0b10
  assert(strcmp(out, "?") == 0 && !pt2262_is_valid(out));

  // A realistic 24-bit remote code decodes to 12 tri-state symbols.
  n = pt2262_tristate(0x00FF0F, 24, out);
  assert(n == 12);
  assert(pt2262_is_valid(out));            // 00 FF 0F -> only 00/11/01 pairs
  return 0;
}
