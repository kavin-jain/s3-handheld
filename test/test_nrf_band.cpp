// Host unit test for the pure 2.4 GHz band helpers.
//   g++ -std=c++17 test/test_nrf_band.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/nrf_band.h"
#include <cassert>

int main() {
  uint8_t c[] = {2, 9, 0, 5, 9};
  assert(nrf_busiest(c, 5) == 1);          // first max (9 at idx 1)
  assert(nrf_busiest(nullptr, 0) == -1);

  assert(nrf_ch_mhz(0) == 2400);
  assert(nrf_ch_mhz(6) == 2406);
  assert(nrf_ch_mhz(125) == 2525);
  return 0;
}
