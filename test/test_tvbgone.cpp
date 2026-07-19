// Host unit test for the TV-B-Gone code table.
//   g++ -std=c++17 test/test_tvbgone.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/tvbgone.h"
#include <cassert>

int main() {
  assert(tvb_count() == 5);
  TvCode c;
  assert(tvb_get(0, &c) && c.proto == 7 && c.value == 0xE0E040BFULL && c.bits == 32);  // Samsung
  assert(tvb_get(2, &c) && c.proto == 4 && c.bits == 12);                              // Sony 12-bit
  assert(!tvb_get(-1, &c));
  assert(!tvb_get(5, &c));      // out of range
  assert(!tvb_get(0, nullptr));
  return 0;
}
