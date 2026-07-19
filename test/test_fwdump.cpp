// Host unit test for the pure SPI-flash JEDEC-ID decode.
//   g++ -std=c++17 test/test_fwdump.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/fwdump.h"
#include <cassert>
#include <cstring>

int main() {
  // W25Q128 = EF 40 18 -> Winbond, 16 MiB.
  assert(strcmp(jedec_manuf(0xEF), "Winbond") == 0);
  assert(jedec_capacity_bytes(0x18) == 16u * 1024 * 1024);
  // 8 MiB and 4 MiB codes.
  assert(jedec_capacity_bytes(0x17) == 8u * 1024 * 1024);
  assert(jedec_capacity_bytes(0x16) == 4u * 1024 * 1024);
  // Other vendors.
  assert(strcmp(jedec_manuf(0xC2), "Macronix") == 0);
  assert(strcmp(jedec_manuf(0x00), "unknown") == 0);
  // Out-of-range capacity code -> 0.
  assert(jedec_capacity_bytes(0x00) == 0);
  assert(jedec_capacity_bytes(0xFF) == 0);
  return 0;
}
