// Host unit test for the pure WPA handshake classifier.
//   g++ -std=c++17 test/test_handshake.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/handshake.h"
#include <cassert>

int main() {
  // Real-world Key Information values (key-descriptor version 2 in low bits).
  assert(eapol_msg_num(0x008a) == 1);   // ACK
  assert(eapol_msg_num(0x010a) == 2);   // MIC
  assert(eapol_msg_num(0x13ca) == 3);   // Install+ACK+MIC+Secure
  assert(eapol_msg_num(0x030a) == 4);   // MIC+Secure
  // Non-handshake / ambiguous flags -> 0.
  assert(eapol_msg_num(0x0000) == 0);
  assert(eapol_msg_num(0x0180) == 0);   // ACK+MIC but no install/secure

  // Crackable once M1 and M2 are both seen.
  assert(!handshake_crackable(0x01));   // only M1
  assert(!handshake_crackable(0x02));   // only M2
  assert(handshake_crackable(0x03));    // M1+M2
  assert(handshake_crackable(0x0F));    // all four
  return 0;
}
