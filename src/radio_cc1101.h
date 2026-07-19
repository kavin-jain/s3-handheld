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
