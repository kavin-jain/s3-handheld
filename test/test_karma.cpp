// Host unit test for the pure 802.11 IE / probe-SSID parser.
//   g++ -std=c++17 test/test_karma.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/karma.h"
#include <cassert>
#include <cstring>

int main() {
  char ssid[33];

  // SSID first: id 0, len 5, "Home!".
  const uint8_t a[] = {0x00,0x05,'H','o','m','e','!', 0x01,0x02,0x82,0x84};
  assert(ie_get_ssid(a, sizeof a, ssid, sizeof ssid));
  assert(strcmp(ssid, "Home!") == 0);

  // SSID after a rates IE — must still be found by walking tags.
  const uint8_t b[] = {0x01,0x04,0x82,0x84,0x8b,0x96, 0x00,0x03,'C','a','f'};
  assert(ie_get_ssid(b, sizeof b, ssid, sizeof ssid));
  assert(strcmp(ssid, "Caf") == 0);

  // Hidden/broadcast probe: zero-length SSID -> false, empty out.
  const uint8_t c[] = {0x00,0x00, 0x01,0x02,0x82,0x84};
  assert(!ie_get_ssid(c, sizeof c, ssid, sizeof ssid));
  assert(ssid[0] == 0);

  // No SSID element present -> false.
  const uint8_t d[] = {0x01,0x02,0x82,0x84, 0x03,0x01,0x06};
  assert(!ie_get_ssid(d, sizeof d, ssid, sizeof ssid));

  // Truncated IE (claims len 9 but buffer ends) -> no crash, false.
  const uint8_t e[] = {0x00,0x09,'x','y'};
  assert(!ie_get_ssid(e, sizeof e, ssid, sizeof ssid));
  return 0;
}
