// Host unit test for the pure NFC key/UID helpers.
//   g++ -std=c++17 test/test_nfc_keys.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/nfc_keys.h"
#include <cassert>
#include <cstring>

int main() {
  uint8_t k[6];
  assert(nfc_parse_key("FFFFFFFFFFFF", k) && k[0] == 0xFF && k[5] == 0xFF);
  assert(nfc_parse_key("a0a1a2a3a4a5", k) && k[0] == 0xA0 && k[5] == 0xA5);  // lowercase
  assert(!nfc_parse_key("# comment", k));
  assert(!nfc_parse_key("", k));
  assert(!nfc_parse_key("FFFF", k));                 // too short
  assert(!nfc_parse_key("FFFFFFFFFFFFFF", k));        // too long (14 hex)
  assert(!nfc_parse_key("ZZZZZZZZZZZZ", k));          // non-hex
  assert(nfc_parse_key("  D3F7D3F7D3F7", k) && k[0] == 0xD3);  // leading ws ok

  char h[24];
  uint8_t uid[] = {0x04, 0xA2, 0x1B};
  assert(nfc_uid_hex(uid, 3, h, sizeof h) > 0 && strcmp(h, "04:A2:1B") == 0);
  uint8_t one[] = {0x9C};
  assert(nfc_uid_hex(one, 1, h, sizeof h) > 0 && strcmp(h, "9C") == 0);
  assert(nfc_uid_hex(uid, 3, h, 3) == 0);            // too small -> safe 0
  return 0;
}
