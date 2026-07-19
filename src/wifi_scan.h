// WiFi recon — native 2.4 GHz scan. Interpretation helpers in wifi_fmt.h (tested).
#pragma once

void        wifi_begin();          // bring up STA mode (lazy; called by wifi_scan too)
int         wifi_scan();           // (re)scan, returns network count (blocks ~2 s)
int         wifi_count();          // last scan's count
const char *wifi_ssid(int i);
int         wifi_rssi(int i);
int         wifi_chan(int i);
int         wifi_enc(int i);       // wifi_auth_mode_t as int (see wifi_fmt.h)
