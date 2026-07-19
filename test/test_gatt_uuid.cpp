// Host unit test for the pure GATT UUID name lookup.
//   g++ -std=c++17 test/test_gatt_uuid.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/gatt_uuid.h"
#include <cassert>
#include <cstring>

int main() {
  assert(strcmp(gatt_service_name(0x180F), "Battery") == 0);
  assert(strcmp(gatt_service_name(0x180D), "Heart Rate") == 0);
  assert(strcmp(gatt_service_name(0x1812), "HID") == 0);
  assert(strcmp(gatt_service_name(0x180A), "Device Info") == 0);
  assert(gatt_service_name(0x1234) == nullptr);
  return 0;
}
