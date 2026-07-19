// Pure global-intensity mapping — host-testable.
// One "intensity" dial (Low/Medium/Max) scales the TX power of every radio so
// the whole device can be dialled from quiet/close-range up to the hardware's
// limit, instead of hard-coding max. Maps the level to each chip's native units.
// The runtime holder is power_ctl.h; the radio wrappers read it.
#pragma once

enum { PWR_LOW = 0, PWR_MED, PWR_MAX, PWR_N };

static inline int pwr_clamp(int lvl) {
  return lvl < 0 ? 0 : lvl >= PWR_N ? PWR_N - 1 : lvl;
}

static inline const char *pwr_name(int lvl) {
  switch (pwr_clamp(lvl)) {
    case PWR_LOW: return "Low";
    case PWR_MED: return "Medium";
    default:      return "Max";
  }
}

// CC1101 setPA() argument in dBm (table-topped at +12).
static inline int pwr_cc1101_dbm(int lvl) {
  static const int t[PWR_N] = {0, 7, 12};
  return t[pwr_clamp(lvl)];
}

// RF24 PA level enum: 1=RF24_PA_LOW, 2=RF24_PA_HIGH, 3=RF24_PA_MAX.
static inline int pwr_nrf24_pa(int lvl) {
  static const int t[PWR_N] = {1, 2, 3};
  return t[pwr_clamp(lvl)];
}

// esp_wifi_set_max_tx_power() units = 0.25 dBm; valid 8..84 (~2..20.5 dBm).
static inline int pwr_wifi_qdbm(int lvl) {
  static const int t[PWR_N] = {34, 60, 84};   // ~8.5 / 15 / 20.5 dBm
  return t[pwr_clamp(lvl)];
}
