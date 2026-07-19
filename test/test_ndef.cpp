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

  // TLV wrap: 0x03 <len> <record> 0xFE.
  size_t rec = ndef_uri_record("https://x.com", b, sizeof b);
  uint8_t tag[64];
  size_t tn = ndef_tlv_wrap(b, rec, tag, sizeof tag);
  assert(tn == rec + 3);
  assert(tag[0] == 0x03 && tag[1] == (uint8_t)rec);
  assert(memcmp(tag + 2, b, rec) == 0);
  assert(tag[tn - 1] == 0xFE);
  assert(ndef_tlv_wrap(b, rec, tag, rec + 2) == 0);        // overflow -> 0
  assert(ndef_tlv_wrap(b, 0, tag, sizeof tag) == 0);       // empty -> 0
  return 0;
}
