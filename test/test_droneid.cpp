// Host unit test for the pure OpenDroneID decode.
//   g++ -std=c++17 test/test_droneid.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/droneid.h"
#include <cassert>
#include <cstring>
#include <cmath>

int main() {
  // Header nibble decode.
  assert(odid_msg_type(0x02) == ODID_BASIC_ID);   // type 0, version 2
  assert(odid_msg_type(0x12) == ODID_LOCATION);   // type 1, version 2

  // int32 LE round-trip + 1e-7 scaling, both signs.
  int32_t lat = 129716000;    // 12.9716 deg
  int32_t lon = -740060000;   // -74.006 deg
  uint8_t buf[8] = {
    (uint8_t)lat, (uint8_t)(lat >> 8), (uint8_t)(lat >> 16), (uint8_t)(lat >> 24),
    (uint8_t)lon, (uint8_t)(lon >> 8), (uint8_t)(lon >> 16), (uint8_t)(lon >> 24)};
  assert(le_i32(buf) == lat);
  assert(le_i32(buf + 4) == lon);
  assert(std::fabs(odid_coord(le_i32(buf)) - 12.9716) < 1e-6);
  assert(std::fabs(odid_coord(le_i32(buf + 4)) + 74.006) < 1e-6);

  // Basic ID string extraction.
  uint8_t msg[22] = {0x02, 0x10};   // header, id/ua type
  const char *id = "1596F3A2C0D9K7X4";
  memcpy(msg + 2, id, strlen(id));
  char out[21];
  odid_basic_id(msg, out);
  assert(strcmp(out, id) == 0);
  return 0;
}
