// Host unit test for the pure ASCII->HID encoder, cross-checked vs the decoder.
//   g++ -std=c++17 test/test_hid_encode.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/hid_encode.h"
#include "../src/hid_keymap.h"
#include <cassert>

int main() {
  // Direct spot-checks.
  bool sh;
  assert(ascii_to_hid('a', &sh) == 0x04 && !sh);
  assert(ascii_to_hid('A', &sh) == 0x04 && sh);
  assert(ascii_to_hid('1', &sh) == 0x1E && !sh);
  assert(ascii_to_hid('!', &sh) == 0x1E && sh);
  assert(ascii_to_hid(' ', &sh) == 0x2C);
  assert(ascii_to_hid('\x01', &sh) == 0);          // unsupported

  // Round-trip every printable char through encode -> decode.
  const char *payload =
    "abcXYZ 0189 !@#$%^&*() -_=+[]{}\\|;:'\",.<>/? Payload_v2!";
  for (const char *p = payload; *p; p++) {
    bool shift;
    uint8_t u = ascii_to_hid(*p, &shift);
    assert(u != 0);
    assert(hid_to_ascii(u, shift) == *p);          // encode then decode = identity
  }
  return 0;
}
