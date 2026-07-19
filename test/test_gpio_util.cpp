// Host unit test for the pure ESP32-S3 GPIO-safety classifier.
//   g++ -std=c++17 test/test_gpio_util.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/gpio_util.h"
#include <cassert>

int main() {
  // Normal usable pins.
  assert(gpio_usable(0) && gpio_usable(5) && gpio_usable(21));
  assert(gpio_usable(38) && gpio_usable(45) && gpio_usable(48));

  // Flash (26..32) + PSRAM (33..37) reserved -> not usable.
  for (int p = 26; p <= 37; p++) { assert(gpio_reserved(p) && !gpio_usable(p)); }
  assert(gpio_usable(25) == false);   // 25 doesn't exist on the S3
  assert(!gpio_valid(22) && !gpio_valid(23) && !gpio_valid(24) && !gpio_valid(25));

  // Out of range.
  assert(!gpio_valid(-1) && !gpio_valid(49) && !gpio_usable(100));

  // Boundary: 38 is the first usable pin above the reserved block.
  assert(!gpio_reserved(38) && gpio_usable(38));
  return 0;
}
