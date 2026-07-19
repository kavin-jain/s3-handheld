// Host unit test for the pure amiibo / NTAG215 helpers.
//   g++ -std=c++17 test/test_amiibo.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/amiibo.h"
#include <cassert>
#include <cstring>

int main() {
  uint8_t d[NTAG215_SIZE] = {0};

  // Not an amiibo until the NTAG215 CC is present; short buffer never is.
  assert(!ntag215_is_amiibo(d, sizeof d));
  assert(!ntag215_is_amiibo(d, 100));
  d[12] = 0xE1; d[13] = 0x10; d[14] = 0x3E; d[15] = 0x00;
  assert(ntag215_is_amiibo(d, sizeof d));

  // Figure id at page 21 (Mario = ...0002).
  const uint8_t mario[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02};
  memcpy(d + AMIIBO_ID_OFF, mario, 8);
  char id[17];
  amiibo_id_hex(d, id);
  assert(strcmp(id, "0000000000000002") == 0);

  // BCC checksums: known vector.
  const uint8_t uid[7] = {0x04,0x7A,0x12,0x9C,0x33,0x55,0x80};
  assert(nfc_bcc0(uid) == 0xE4);   // 0x88^04^7A^12
  assert(nfc_bcc1(uid) == 0x7A);   // 9C^33^55^80

  // Lay that UID into the dump with correct BCCs -> valid; corrupt one -> invalid.
  d[0]=uid[0]; d[1]=uid[1]; d[2]=uid[2]; d[3]=nfc_bcc0(uid);
  d[4]=uid[3]; d[5]=uid[4]; d[6]=uid[5]; d[7]=uid[6]; d[8]=nfc_bcc1(uid);
  assert(ntag215_uid_valid(d, sizeof d));
  d[3] ^= 0xFF;
  assert(!ntag215_uid_valid(d, sizeof d));
  return 0;
}
