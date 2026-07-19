// Host unit test for the pure BLE tracker heuristics.
//   g++ -std=c++17 test/test_ble_track.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ble_track.h"
#include <cassert>
#include <cstring>

int main() {
  uint8_t airtag[] = {0x4C, 0x00, 0x12, 0x19};
  assert(ble_is_airtag(airtag, 4));
  uint8_t apple_nearby[] = {0x4C, 0x00, 0x07};   // Apple but not offline-finding
  assert(!ble_is_airtag(apple_nearby, 3));
  uint8_t other[] = {0x75, 0x00, 0x12};          // Samsung company id
  assert(!ble_is_airtag(other, 3));
  assert(!ble_is_airtag(airtag, 2));             // too short
  assert(!ble_is_airtag(nullptr, 4));

  assert(strcmp(ble_tracker_by_uuid(0xFEED), "Tile") == 0);
  assert(strcmp(ble_tracker_by_uuid(0xFD5A), "Samsung SmartTag") == 0);
  assert(ble_tracker_by_uuid(0x1234) == nullptr);
  return 0;
}
