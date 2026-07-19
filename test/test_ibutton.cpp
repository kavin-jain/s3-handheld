// Host unit test for the pure 1-Wire CRC8 + iButton validation.
//   g++ -std=c++17 test/test_ibutton.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ibutton.h"
#include <cassert>

int main() {
  // Self-consistency: fill the CRC byte, then validation must pass.
  uint8_t rom[8] = {0x01, 0x2A, 0x3B, 0x4C, 0x5D, 0x6E, 0x7F, 0x00};
  rom[7] = onewire_crc8(rom, 7);
  assert(ibutton_valid(rom));

  rom[3] ^= 0xFF;                 // corrupt a byte -> invalid
  assert(!ibutton_valid(rom));

  // CRC8 is deterministic and non-trivial.
  uint8_t a[] = {0x02};
  uint8_t b[] = {0x03};
  assert(onewire_crc8(a, 1) != onewire_crc8(b, 1));
  assert(onewire_crc8(a, 0) == 0);   // empty -> 0
  return 0;
}
