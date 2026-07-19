// Host unit test for the pure Flipper .ir record parser.
//   g++ -std=c++17 test/test_flipper_ir.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/flipper_ir.h"
#include <cassert>
#include <cstring>

int main() {
  // Byte-field parse.
  uint8_t b[4];
  assert(flipper_bytes("04 00 00 00", b, 4) == 4);
  assert(b[0] == 0x04 && b[1] == 0x00);
  assert(flipper_bytes("E0 E0", b, 4) == 2 && b[0] == 0xE0);

  // A parsed record (the common case).
  const char *rec =
    "name: Power\n"
    "type: parsed\n"
    "protocol: NEC\n"
    "address: 04 00 00 00\n"
    "command: 08 00 00 00\n";
  FlipperIr fp;
  assert(flipper_ir_parse(rec, &fp));
  assert(strcmp(fp.name, "Power") == 0);
  assert(fp.type == IRREC_PARSED);
  assert(strcmp(fp.protocol, "NEC") == 0);
  assert(fp.addr_len == 4 && fp.addr[0] == 0x04);
  assert(fp.cmd_len == 4 && fp.cmd[0] == 0x08);

  // A raw record: type detected, no protocol.
  const char *raw =
    "name: Vol_up\n"
    "type: raw\n"
    "frequency: 38000\n"
    "duty_cycle: 0.330000\n"
    "data: 9000 4500 560 560\n";
  assert(flipper_ir_parse(raw, &fp));
  assert(fp.type == IRREC_RAW && strcmp(fp.name, "Vol_up") == 0);

  // Junk without a name/type is rejected.
  assert(!flipper_ir_parse("Filetype: IR signals file\nVersion: 1\n", &fp));
  return 0;
}
