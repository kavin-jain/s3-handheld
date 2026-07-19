// CC1101 #1 (sub-GHz) — thin wrapper over the SmartRC/ELECHOUSE driver, on SPI-B.
// Only radio #1 for now; the other CC1101 + 2×NRF24 share the bus and come later.
#pragma once
#include <stdint.h>

bool    cc1101_begin();                 // init on SPI-B pins; true if the chip answers
bool    cc1101_present();
uint8_t cc1101_version();               // VERSION status register (diagnostic)
int     cc1101_rssi_at(float mhz);      // tune, RX, settle, read dBm

// Read RSSI at each freq into rssi_out[0..n); return index of the strongest, or -1.
int     cc1101_sweep(const float *freqs, int n, int *rssi_out);

// ---- Fixed-code capture & replay (async OOK on GDO0) — see subghz_replay.h ----
// Tune to mhz, listen up to timeout_ms for a decodable fob/gate frame. On success
// fills code/bits/proto_no (1-based) and returns true. Radio-dependent: bring-up.
bool    subghz_capture(float mhz, uint32_t timeout_ms,
                       uint32_t *code, uint8_t *bits, int *proto_no);
// Re-transmit a previously captured code on mhz. Bring-up (async TX).
bool    subghz_replay(float mhz, uint32_t code, uint8_t bits, int proto_no);
