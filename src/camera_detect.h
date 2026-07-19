// Pure hidden-camera heuristic — flag WiFi AP names that look like IP cameras.
// Host-testable. ponytail: name-matching is a heuristic; BSSID/OUI vendor lookup
// is the stronger signal — add it once wifi_scan exposes the BSSID.
#pragma once
#include <stddef.h>
#include "strutil.h"

// Returns the matched pattern (brand-ish) or nullptr.
static inline const char *camera_ssid_brand(const char *ssid) {
  static const char *pats[] = {"ipcam", "ip-cam", "wificam", "cctv", "v380",
                               "goke", "hi3518", "wyze", "reolink", "ezviz",
                               "yi-", "webcam"};
  for (size_t k = 0; k < sizeof(pats) / sizeof(pats[0]); k++)
    if (ci_contains(ssid, pats[k])) return pats[k];
  return nullptr;
}

static inline bool is_camera_ssid(const char *ssid) {
  return camera_ssid_brand(ssid) != nullptr;
}
