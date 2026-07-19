// Pure sub-GHz interpretation logic — no Arduino, so it host-unit-tests.
// "Show what the signal is, not the raw number": turn a MHz reading into a
// plain-language guess, and pick the strongest channel from a sweep.
#pragma once
#include <stddef.h>

// Index of the strongest RSSI (dBm: higher/less-negative = stronger). -1 if empty.
static inline int sg_peak(const int *rssi, int n) {
  if (!rssi || n <= 0) return -1;
  int b = 0;
  for (int i = 1; i < n; i++) if (rssi[i] > rssi[b]) b = i;
  return b;
}

// Plain-language guess for what typically lives on a sub-GHz frequency (MHz).
static inline const char *sg_guess(float mhz) {
  if (mhz >= 300 && mhz <= 322) return "US remote / TPMS / key fob";
  if (mhz >= 386 && mhz <= 445) return "Remote / gate / car fob / sensor";
  if (mhz >= 862 && mhz <= 872) return "EU ISM: sensor / LoRa / meter";
  if (mhz >= 900 && mhz <= 930) return "US ISM: LoRa / sensor";
  return "Unknown sub-GHz band";
}

// Map an RSSI in dBm (~ -100 weak .. -30 strong) to a 0..100 bar value.
static inline int sg_bar_pct(int dbm) {
  int p = dbm + 100;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}
