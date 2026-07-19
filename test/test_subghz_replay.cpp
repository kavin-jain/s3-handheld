// Host unit test for the pure RCSwitch codec.
//   g++ -std=c++17 test/test_subghz_replay.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/subghz_replay.h"
#include <cassert>
#include <cstring>

int main() {
  // Round-trip: encode each bit at spec timing, decoder must recover it.
  for (int pi = 0; pi < RCS_PROTO_COUNT; ++pi) {
    const RcsProto &p = RCS_PROTOS[pi];
    for (int b = 0; b <= 1; ++b) {
      uint32_t hi, lo;
      rcs_symbol(b, &p, &hi, &lo);
      assert(rcs_bit(hi, lo, &p, 20) == b);
    }
  }

  // Tolerance: a 10% skewed "one" on protocol 1 (1050/350) still decodes.
  const RcsProto &p1 = RCS_PROTOS[0];
  assert(rcs_bit(1155, 315, &p1, 20) == 1);       // +10% hi, -10% lo
  // Garbage timing matches nothing.
  assert(rcs_bit(700, 700, &p1, 20) == -1);

  // Base pulse recovered from the sync gap: sync_lo=31 -> 31*350=10850us.
  assert(rcs_base_from_sync(10850, &p1) == 350);

  // Full-frame decode: synthesize a protocol-1 capture for a known code, then
  // decode it back. d[0] = sync-low gap, then high,low per bit MSB-first.
  const uint32_t CODE = 0xA5C;                   // 12 bits: 1010 0101 1100
  const int NB = 12;
  uint32_t frame[1 + 2 * NB];
  frame[0] = (uint32_t)p1.base_us * p1.sync_lo;  // 350 * 31
  for (int i = 0; i < NB; ++i) {
    int bit = (CODE >> (NB - 1 - i)) & 1;
    rcs_symbol(bit, &p1, &frame[1 + 2 * i], &frame[2 + 2 * i]);
  }
  uint32_t code; uint8_t bits; int proto;
  assert(rcs_decode(frame, 1 + 2 * NB, 20, &code, &bits, &proto));
  assert(code == CODE && bits == NB && proto == 1);

  // Noise-only buffer decodes to nothing.
  uint32_t junk[9] = {5000, 800, 800, 800, 800, 800, 800, 800, 800};
  assert(!rcs_decode(junk, 9, 20, &code, &bits, &proto));

  // Formatting.
  char buf[40];
  rcs_fmt(0x15F3, 24, 1, buf, sizeof buf);
  assert(strcmp(buf, "0x0015F3  (24 bit, P1)") == 0);
  return 0;
}
