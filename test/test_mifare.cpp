// Host unit test for the pure MIFARE default-key dictionary.
//   g++ -std=c++17 test/test_mifare.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/mifare.h"
#include <cassert>
#include <cstring>

int main() {
  assert(MIFARE_KEY_COUNT >= 6);

  const uint8_t ff[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  assert(strcmp(mifare_key_name(ff), "factory default") == 0);

  const uint8_t mad[6] = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5};
  assert(strcmp(mifare_key_name(mad), "MAD key A") == 0);

  const uint8_t ndef[6] = {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7};
  assert(strcmp(mifare_key_name(ndef), "NDEF") == 0);

  const uint8_t zero[6] = {0,0,0,0,0,0};
  assert(strcmp(mifare_key_name(zero), "all-zero") == 0);

  const uint8_t rnd[6] = {0x12,0x34,0x56,0x78,0x9A,0xBC};
  assert(strcmp(mifare_key_name(rnd), "unknown") == 0);

  // Every table entry is a parseable 6-byte key.
  for (int i = 0; i < MIFARE_KEY_COUNT; i++) {
    uint8_t k[6];
    assert(nfc_parse_key(MIFARE_DEFAULT_KEYS[i].hex, k));
  }
  return 0;
}
