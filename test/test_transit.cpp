// Host unit test for the pure transit helpers.
//   g++ -std=c++17 test/test_transit.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/transit.h"
#include <cassert>
#include <cstring>

int main() {
  uint8_t blk[16] = {0x39, 0x30, 0x00, 0x00};   // 0x3039 = 12345 LE
  assert(le_u32(blk, 0) == 12345);
  uint8_t b2[8] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0};
  assert(le_u32(b2, 1) == 0xFFFFFFFFu);

  char r[16];
  assert(fmt_rupees(24550, r, sizeof r) && strcmp(r, "Rs 245.50") == 0);
  assert(fmt_rupees(5, r, sizeof r) && strcmp(r, "Rs 0.05") == 0);
  assert(fmt_rupees(100, r, sizeof r) && strcmp(r, "Rs 1.00") == 0);
  return 0;
}
