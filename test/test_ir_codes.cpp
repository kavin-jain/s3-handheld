// Host unit test for the pure Flipper-.ir parsing helpers.
//   g++ -std=c++17 test/test_ir_codes.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ir_codes.h"
#include <cassert>
#include <cstring>

int main() {
  char v[32];
  assert(ir_kv("protocol: NEC", "protocol", v, sizeof v) && strcmp(v, "NEC") == 0);
  assert(ir_kv("address: 04 00 00 00", "address", v, sizeof v) &&
         strcmp(v, "04 00 00 00") == 0);
  assert(ir_kv("  name:  Power  ", "name", v, sizeof v) && strcmp(v, "Power") == 0);
  assert(!ir_kv("command: 08", "protocol", v, sizeof v));   // wrong key
  assert(!ir_kv("protocolX: NEC", "protocol", v, sizeof v)); // key must be followed by ':'

  assert(ir_first_byte("04 00 00 00") == 0x04);
  assert(ir_first_byte("A0 1B") == 0xA0);
  assert(ir_first_byte("  ff") == 0xFF);
  assert(ir_first_byte("zz") == -1);
  assert(ir_first_byte("") == -1);
  return 0;
}
