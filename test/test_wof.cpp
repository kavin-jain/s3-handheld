// Host unit test for Wall-of-Flipper identification.
//   g++ -std=c++17 test/test_wof.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/wof.h"
#include <cassert>
#include <cstring>

int main() {
  assert(strcmp(wof_identify("Flipper Zorro"), "Flipper Zero") == 0);
  assert(strcmp(wof_identify("pwnagotchi-a1b2"), "Pwnagotchi") == 0);
  assert(strcmp(wof_identify("ESP32 Marauder"), "WiFi Marauder") == 0);
  assert(wof_identify("iPhone") == nullptr);
  assert(wof_identify("") == nullptr);
  assert(wof_identify(nullptr) == nullptr);
  return 0;
}
