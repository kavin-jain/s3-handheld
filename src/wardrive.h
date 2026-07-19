// Pure WiGLE-CSV wardrive row formatter — host-testable.
// Wardriving logs every AP seen along with a GPS fix; WiGLE's CSV is the de-facto
// upload format. The WiFi scan + TinyGPSPlus fix + SD append are bring-up; this
// file just formats one row so the schema is pinned and tested.
#pragma once
#include <stddef.h>
#include <stdio.h>

// One WiGLE row: MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,Lat,Lon,Alt,Accuracy,Type.
// FirstSeen is left blank (filled from the RTC on device). Returns snprintf length.
static inline int wardrive_csv(const char *bssid, const char *ssid,
                               const char *auth, int channel, int rssi,
                               double lat, double lon, char *out, size_t cap) {
  return snprintf(out, cap, "%s,%s,%s,,%d,%d,%.6f,%.6f,0,0,WIFI",
                  bssid, ssid, auth, channel, rssi, lat, lon);
}
