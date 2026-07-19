// Host unit test for the pure ESP-NOW framing.
//   g++ -std=c++17 test/test_espnow_msg.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/espnow_msg.h"
#include <cassert>
#include <cstring>

int main() {
  uint8_t f[64];
  size_t n = espnow_frame("ping", f, sizeof f);
  assert(n == 7 && f[0] == ESPNOW_MAGIC && f[1] == 4);

  char t[64];
  assert(espnow_parse(f, n, t, sizeof t) == 4 && strcmp(t, "ping") == 0);   // round-trip

  f[0] = 0x00;                                   // corrupt magic
  assert(espnow_parse(f, n, t, sizeof t) == 0);
  f[0] = ESPNOW_MAGIC;
  f[3] ^= 0xFF;                                  // corrupt a payload byte -> checksum fail
  assert(espnow_parse(f, n, t, sizeof t) == 0);

  assert(espnow_frame("x", f, 3) == 0);          // overflow (need 4)
  return 0;
}
