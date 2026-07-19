// Host unit test for the pure 802.11 deauth frame builder.
//   g++ -std=c++17 test/test_deauth_frame.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/deauth_detect.h"
#include <cassert>
#include <cstring>

int main() {
  const uint8_t client[6] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
  const uint8_t bssid[6]  = {0x00,0x11,0x22,0x33,0x44,0x55};
  uint8_t f[DEAUTH_FRAME_LEN];
  deauth_frame(client, bssid, 7, f);

  // Frame control marks a deauth — the detector must recognise our own frame.
  assert(f[0] == 0xC0 && f[1] == 0x00);
  assert(is_deauth(f, sizeof f));

  // addr1 = destination, addr2 & addr3 = the AP BSSID.
  assert(memcmp(f + 4,  client, 6) == 0);
  assert(memcmp(f + 10, bssid, 6) == 0);
  assert(memcmp(f + 16, bssid, 6) == 0);

  // Reason code is little-endian.
  assert(f[24] == 0x07 && f[25] == 0x00);

  // Broadcast target is a valid frame too.
  const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  deauth_frame(bcast, bssid, 1, f);
  assert(memcmp(f + 4, bcast, 6) == 0 && f[24] == 0x01);

  // Explicit-source deauth (client -> AP direction): addr1=AP, addr2=client.
  deauth_frame_ex(bssid, client, bssid, 7, f);
  assert(f[0] == 0xC0 && is_deauth(f, sizeof f));
  assert(memcmp(f + 4, bssid, 6) == 0);        // addr1 = AP
  assert(memcmp(f + 10, client, 6) == 0);      // addr2 = client (source)
  assert(memcmp(f + 16, bssid, 6) == 0);       // addr3 = BSSID

  // Disassoc frame: subtype 0xA0, still recognised by the detector.
  disassoc_frame_ex(client, bssid, bssid, 7, f);
  assert(f[0] == 0xA0 && is_deauth(f, sizeof f));
  assert(memcmp(f + 4, client, 6) == 0 && memcmp(f + 10, bssid, 6) == 0);
  return 0;
}
