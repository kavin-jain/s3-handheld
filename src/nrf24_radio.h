// NRF24 #1 (2.4 GHz) band scanner — carrier activity per channel. On SPI-B.
// Pure pick/convert helpers in nrf_band.h (host-tested).
#pragma once
#include <stdint.h>

#define NRF_CHAN 40                 // scan the lower 40 channels (2400..2439 MHz)

bool    nrf_present();              // lazy init on first call; true if the chip answers
void    nrf_scan();                 // carrier sweep (cached) -- ~104ms, blocking.
                                     // Call only from the radio task (radio_task.h),
                                     // never from the UI task -- see nrf_scan_request().
bool    nrf_scanned();
int     nrf_busiest_ch();
uint8_t nrf_activity(int ch);

// Async pair for the UI: request a sweep, then poll nrf_scanned() on a timer
// (same cached-globals pattern wifi_scan_async/ble_scan_async already use).
// The actual nrf_scan() runs on the radio task, never on the UI task, so the
// ~104ms sweep can never freeze input/animation again.
void    nrf_scan_request();
void    nrf_scan_service();         // radio task only: runs a requested sweep, if any

// Mousejack: inject a keystroke stream (mousejack_stream frames, 10 B each) at a
// sniffed dongle address on the given channel. Bring-up. Authorized/own-gear only.
bool    nrf_mousejack_inject(const uint8_t addr[5], int channel,
                             const uint8_t frames[][10], int n);
