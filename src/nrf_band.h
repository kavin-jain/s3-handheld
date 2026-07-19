// Pure 2.4 GHz band-scan helpers — host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Index of the busiest channel (highest hit count). -1 if empty.
static inline int nrf_busiest(const uint8_t *counts, int n) {
  if (!counts || n <= 0) return -1;
  int b = 0;
  for (int i = 1; i < n; i++) if (counts[i] > counts[b]) b = i;
  return b;
}

// nRF24 channel (0..125) -> centre frequency in MHz.
static inline int nrf_ch_mhz(int ch) { return 2400 + ch; }
