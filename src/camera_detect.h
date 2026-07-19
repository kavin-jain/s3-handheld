// Pure hidden-camera heuristic — flag WiFi AP names that look like IP cameras.
// Host-testable. ponytail: name-matching is a heuristic; BSSID/OUI vendor lookup
// is the stronger signal — add it once wifi_scan exposes the BSSID.
#pragma once
#include <stddef.h>

static inline char cam_lc(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static inline bool cam_ci_contains(const char *hay, const char *needle) {
  if (!hay || !needle) return false;
  for (size_t i = 0; hay[i]; i++) {
    size_t j = 0;
    while (needle[j] && cam_lc(hay[i + j]) == cam_lc(needle[j])) j++;
    if (!needle[j]) return true;
  }
  return false;
}

// Returns the matched pattern (brand-ish) or nullptr.
static inline const char *camera_ssid_brand(const char *ssid) {
  static const char *pats[] = {"ipcam", "ip-cam", "wificam", "cctv", "v380",
                               "goke", "hi3518", "wyze", "reolink", "ezviz",
                               "yi-", "webcam"};
  for (size_t k = 0; k < sizeof(pats) / sizeof(pats[0]); k++)
    if (cam_ci_contains(ssid, pats[k])) return pats[k];
  return nullptr;
}

static inline bool is_camera_ssid(const char *ssid) {
  return camera_ssid_brand(ssid) != nullptr;
}
