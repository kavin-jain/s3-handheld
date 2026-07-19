// Host unit test for the pure NDEF URI record builder.
//   g++ -std=c++17 test/test_ndef.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ndef.h"
#include <cassert>
#include <cstring>

int main() {
  uint8_t b[64];
  size_t n = ndef_uri_record("https://x.com", b, sizeof b);
  assert(n == 10);                          // 4 hdr + 1 prefix + 5 uri
  assert(b[0] == 0xD1 && b[1] == 0x01 && b[2] == 6 && b[3] == 'U' && b[4] == 0x04);
  assert(memcmp(b + 5, "x.com", 5) == 0);

  n = ndef_uri_record("http://www.a.io", b, sizeof b);
  assert(b[4] == 0x01 && memcmp(b + 5, "a.io", 4) == 0);   // http://www. prefix

  n = ndef_uri_record("ftp://x", b, sizeof b);
  assert(b[4] == 0x00);                     // no known prefix -> full URI

  assert(ndef_uri_record("https://x.com", b, 5) == 0);     // overflow -> 0
  assert(ndef_uri_record(nullptr, b, sizeof b) == 0);
  return 0;
}
