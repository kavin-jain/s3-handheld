// Host unit test for the pure global-intensity mapping.
//   g++ -std=c++17 test/test_power_level.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/power_level.h"
#include <cassert>
#include <cstring>

int main() {
  // Names.
  assert(strcmp(pwr_name(PWR_LOW), "Low") == 0);
  assert(strcmp(pwr_name(PWR_MED), "Medium") == 0);
  assert(strcmp(pwr_name(PWR_MAX), "Max") == 0);

  // Monotonic per radio, maxing at the hardware limit.
  assert(pwr_cc1101_dbm(PWR_LOW) == 0 && pwr_cc1101_dbm(PWR_MAX) == 12);
  assert(pwr_cc1101_dbm(PWR_LOW) < pwr_cc1101_dbm(PWR_MED));
  assert(pwr_nrf24_pa(PWR_LOW) == 1 && pwr_nrf24_pa(PWR_MAX) == 3);   // PA_LOW..PA_MAX
  assert(pwr_wifi_qdbm(PWR_MAX) == 84 && pwr_wifi_qdbm(PWR_LOW) == 34);
  assert(pwr_wifi_qdbm(PWR_LOW) < pwr_wifi_qdbm(PWR_MED));

  // Out-of-range clamps (never index past the table).
  assert(pwr_clamp(-5) == PWR_LOW && pwr_clamp(99) == PWR_MAX);
  assert(pwr_cc1101_dbm(99) == 12 && pwr_wifi_qdbm(-1) == 34);
  return 0;
}
