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

  // Company-id extraction (LE) + brand lookup.
  assert(ble_company_id(airtag, 4) == 0x004C);
  assert(ble_company_id(other, 3) == 0x0075);
  assert(ble_company_id(airtag, 1) == 0);        // too short
  assert(strcmp(ble_tracker_brand(0x004C), "Apple AirTag") == 0);
  assert(strcmp(ble_tracker_brand(0x0075), "Samsung SmartTag") == 0);
  assert(strcmp(ble_tracker_brand(0x0157), "Tile") == 0);
  assert(ble_tracker_brand(0x0059) == nullptr);
  return 0;
}
