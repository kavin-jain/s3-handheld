// Pure 802.11 Information Element parser for Karma/MANA — host-testable.
// Karma listens for probe requests and answers with whatever SSID a client asks
// for, luring it to auto-associate. The trick is reading the SSID out of the
// probe's tagged parameters. The SoftAP beacon/response is bring-up; this file
// is the IE walk. For auditing your own devices only.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Walk tagged IEs (id, len, data...) and copy the SSID (element id 0) into out.
// Returns true only for a real (non-hidden) SSID; a zero-length SSID IE is a
// broadcast/hidden probe and yields false with out = "".
static inline bool ie_get_ssid(const uint8_t *ies, int len,
                               char *out, size_t cap) {
  if (!ies || !out || cap == 0) return false;
  out[0] = 0;
  for (int i = 0; i + 1 < len; ) {
    uint8_t id = ies[i], l = ies[i + 1];
    if (i + 2 + (int)l > len) break;             // truncated IE -> stop
    if (id == 0) {                               // SSID element
      size_t n = l < cap - 1 ? l : cap - 1;
      memcpy(out, ies + i + 2, n);
      out[n] = 0;
      return l > 0;                              // hidden (len 0) -> not a target
    }
    i += 2 + l;
  }
  return false;
}
