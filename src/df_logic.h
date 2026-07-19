// Pure direction-finding trend logic — host-testable.
#pragma once

// Compare current RSSI to the previous sample. +1 warmer (stronger), -1 colder,
// 0 steady. 2 dB deadband to ignore noise.
static inline int df_trend(int rssi, int prev) {
  if (rssi > prev + 2) return 1;
  if (rssi < prev - 2) return -1;
  return 0;
}

static inline const char *df_label(int trend) {
  return trend > 0 ? "WARMER" : trend < 0 ? "COLDER" : "steady";
}
