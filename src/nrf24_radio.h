// NRF24 #1 (2.4 GHz) band scanner — carrier activity per channel. On SPI-B.
// Pure pick/convert helpers in nrf_band.h (host-tested).
#pragma once
#include <stdint.h>

#define NRF_CHAN 40                 // scan the lower 40 channels (2400..2439 MHz)

bool    nrf_present();              // lazy init on first call; true if the chip answers
void    nrf_scan();                 // carrier sweep (cached)
bool    nrf_scanned();
int     nrf_busiest_ch();
uint8_t nrf_activity(int ch);
