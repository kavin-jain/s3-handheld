// Host unit test for the pure WiGLE-CSV wardrive formatter.
//   g++ -std=c++17 test/test_wardrive.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/wardrive.h"
#include <cassert>
#include <cstring>

int main() {
  char row[128];
  int n = wardrive_csv("AA:BB:CC:DD:EE:FF", "HomeNet", "[WPA2-PSK-CCMP][ESS]",
                       6, -47, 12.971600, 77.594600, row, sizeof row);
  assert(n > 0 && (size_t)n < sizeof row);
  assert(strcmp(row,
    "AA:BB:CC:DD:EE:FF,HomeNet,[WPA2-PSK-CCMP][ESS],,6,-47,12.971600,77.594600,0,0,WIFI") == 0);

  // Open network, negative-longitude fix.
  wardrive_csv("00:11:22:33:44:55", "cafe", "[ESS]", 11, -70,
               40.712800, -74.006000, row, sizeof row);
  assert(strcmp(row,
    "00:11:22:33:44:55,cafe,[ESS],,11,-70,40.712800,-74.006000,0,0,WIFI") == 0);
  return 0;
}
