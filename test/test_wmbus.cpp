// Host unit test for the pure wM-Bus header decode.
//   g++ -std=c++17 test/test_wmbus.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/wmbus.h"
#include <cassert>
#include <cstring>

int main() {
  // Manufacturer flag: "ELS" (Elster) = E<<10 | L<<5 | S = 5,12,19 -> 0x1593.
  assert(wmbus_manuf_id("ELS") == 0x1593);
  char m[4];
  wmbus_manuf(0x1593, m);
  assert(strcmp(m, "ELS") == 0);
  // Round-trip a few more flags.
  const char *flags[] = {"ABC", "QDS", "KAM", "ZRI"};
  for (const char *f : flags) {
    wmbus_manuf(wmbus_manuf_id(f), m);
    assert(strcmp(m, f) == 0);
  }

  // Medium table.
  assert(strcmp(wmbus_medium(0x07), "Water") == 0);
  assert(strcmp(wmbus_medium(0x03), "Gas") == 0);
  assert(strcmp(wmbus_medium(0xFF), "Unknown") == 0);

  // CRC-16 invariants: empty buffer -> init(0) XOR 0xFFFF = 0xFFFF.
  assert(wmbus_crc(nullptr, 0) == 0xFFFF);
  // Deterministic and bit-sensitive: one flipped byte changes the CRC.
  uint8_t data[] = {0x44, 0x93, 0x15, 0x78, 0x56, 0x34, 0x12, 0x01, 0x07};
  uint16_t c = wmbus_crc(data, sizeof data);
  assert(wmbus_crc(data, sizeof data) == c);          // stable
  data[3] ^= 0x01;
  assert(wmbus_crc(data, sizeof data) != c);          // detects corruption
  return 0;
}
