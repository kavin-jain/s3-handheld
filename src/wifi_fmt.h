// Pure WiFi interpretation — encryption label + signal quality. Host-testable.
// enc is a wifi_auth_mode_t taken as int (0=OPEN,1=WEP,2=WPA,3=WPA2,4=WPA/2,
// 5=WPA2-Enterprise,6=WPA3,7=WPA2/3), matching esp_wifi's enum order.
#pragma once

static inline int wifi_quality(int rssi) {   // 0..4 bars from dBm
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

static inline const char *wifi_enc_str(int enc) {
  switch (enc) {
    case 0: return "OPEN";
    case 1: return "WEP";
    case 2: return "WPA";
    case 3: return "WPA2";
    case 4: return "WPA/2";
    case 5: return "WPA2-E";
    case 6: return "WPA3";
    case 7: return "WPA2/3";
    default: return "?";
  }
}

static inline bool wifi_is_open(int enc) { return enc == 0; }  // the risky one
