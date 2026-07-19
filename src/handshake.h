// Pure WPA 4-way-handshake classifier — host-testable.
// To crack WPA2 offline you capture the EAPOL-Key frames of the 4-way handshake
// (M1..M4) or a single PMKID. This file identifies which message an EAPOL-Key
// is from its Key Information flags, so the tool can show handshake progress.
// The promiscuous capture + SD write is bring-up. Authorized use only.
#pragma once
#include <stdint.h>

// 802.11 EAPOL-Key "Key Information" field flag bits (the low 3 bits are the
// key-descriptor version and are ignored here).
#define EAPOL_KEYTYPE 0x0008   // 1 = pairwise
#define EAPOL_INSTALL 0x0040
#define EAPOL_ACK     0x0080
#define EAPOL_MIC     0x0100
#define EAPOL_SECURE  0x0200

// Which message (1..4) of the 4-way handshake a Key Information field is; 0 = none.
static inline int eapol_msg_num(uint16_t ki) {
  bool ack  = ki & EAPOL_ACK;
  bool mic  = ki & EAPOL_MIC;
  bool inst = ki & EAPOL_INSTALL;
  bool sec  = ki & EAPOL_SECURE;
  if ( ack && !mic && !inst && !sec) return 1;   // AP->STA: ANonce
  if (!ack &&  mic && !inst && !sec) return 2;   // STA->AP: SNonce + MIC
  if ( ack &&  mic &&  inst &&  sec) return 3;   // AP->STA: install GTK
  if (!ack &&  mic && !inst &&  sec) return 4;   // STA->AP: ack
  return 0;
}

// M2 (or M3) carries the MIC needed to crack; a full M1+M2 pair is enough.
static inline bool handshake_crackable(uint8_t got_msgs_bitmask) {
  return (got_msgs_bitmask & 0x03) == 0x03;      // bits 0,1 = M1,M2 present
}
