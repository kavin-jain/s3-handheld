// Host unit test for the pure 802.11 deauth-frame check.
//   g++ -std=c++17 test/test_deauth_detect.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/deauth_detect.h"
#include <cassert>

int main() {
  uint8_t deauth[]   = {0xC0, 0x00};
  uint8_t disassoc[] = {0xA0, 0x00};
  uint8_t beacon[]   = {0x80, 0x00};   // mgmt beacon (subtype 1000) — not an attack
  uint8_t data[]     = {0x08, 0x00};   // data frame
  assert(is_deauth(deauth, 2));
  assert(is_deauth(disassoc, 2));
  assert(!is_deauth(beacon, 2));
  assert(!is_deauth(data, 2));
  assert(!is_deauth(nullptr, 2));
  assert(!is_deauth(deauth, 0));
  return 0;
}
