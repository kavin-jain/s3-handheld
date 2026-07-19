// Host unit test for the pure EMV public-data decode.
//   g++ -std=c++17 test/test_emv.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/emv.h"
#include <cassert>
#include <cstring>

int main() {
  // Luhn: Visa test PAN is valid; flip the last digit -> invalid.
  assert(luhn_valid("4111111111111111"));
  assert(!luhn_valid("4111111111111112"));
  assert(!luhn_valid(""));
  assert(!luhn_valid("41a1"));

  // Network from BIN.
  assert(strcmp(card_network("4111111111111111"), "Visa") == 0);
  assert(strcmp(card_network("371449635398431"),  "Amex") == 0);
  assert(strcmp(card_network("5500005555555559"), "Mastercard") == 0);

  // Track-2-Equivalent: PAN 'D' 2512 (expiry) 201 (service). 24 nibbles = 12 B.
  const uint8_t t2[12] = {0x41,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
                          0xD2,0x51,0x22,0x01};
  char pan[24], yymm[5];
  assert(emv_parse_track2(t2, sizeof t2, pan, sizeof pan, yymm));
  assert(strcmp(pan, "4111111111111111") == 0);
  assert(strcmp(yymm, "2512") == 0);
  assert(luhn_valid(pan));

  // Padded (odd) form still parses: PAN 'D' 2601 then 0xF pad.
  const uint8_t t2b[11] = {0x41,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
                           0xD2,0x60,0x1F};
  assert(emv_parse_track2(t2b, sizeof t2b, pan, sizeof pan, yymm));
  assert(strcmp(pan, "4111111111111111") == 0);
  assert(strcmp(yymm, "2601") == 0);
  return 0;
}
