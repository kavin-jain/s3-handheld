// Host unit test for the pure WiFi interpretation helpers.
//   g++ -std=c++17 test/test_wifi_fmt.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/wifi_fmt.h"
#include <cassert>
#include <cstring>

int main() {
  assert(wifi_quality(-40) == 4);
  assert(wifi_quality(-60) == 3);
  assert(wifi_quality(-70) == 2);
  assert(wifi_quality(-99) == 0);

  assert(strcmp(wifi_enc_str(0), "OPEN") == 0);
  assert(strcmp(wifi_enc_str(3), "WPA2") == 0);
  assert(strcmp(wifi_enc_str(6), "WPA3") == 0);
  assert(strcmp(wifi_enc_str(42), "?") == 0);

  assert(wifi_is_open(0));
  assert(!wifi_is_open(3));
  return 0;
}
