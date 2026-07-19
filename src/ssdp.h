// Pure SSDP/DIAL response parsing for the cast-crasher prank — host-testable.
// Discovering a Chromecast/Roku means sending an SSDP M-SEARCH and reading the
// LOCATION + ST headers of the reply. The UDP multicast is bring-up; this file
// pulls a header value out and names the device kind.
#pragma once
#include "strutil.h"
#include <stddef.h>

// Extract an HTTP/SSDP header value (case-insensitive key, up to CRLF) into out.
static inline bool ssdp_header(const char *resp, const char *key,
                               char *out, size_t cap) {
  if (!resp || !key || !out || cap == 0) return false;
  out[0] = 0;
  size_t klen = 0; while (key[klen]) klen++;
  for (const char *line = resp; line && *line; ) {
    size_t j = 0;
    while (j < klen && str_lc(line[j]) == str_lc(key[j])) j++;
    if (j == klen && line[j] == ':') {
      const char *v = line + klen + 1;
      while (*v == ' ' || *v == '\t') v++;
      size_t o = 0;
      while (*v && *v != '\r' && *v != '\n' && o + 1 < cap) out[o++] = *v++;
      out[o] = 0;
      return true;
    }
    const char *nl = line; while (*nl && *nl != '\n') nl++;
    line = (*nl == '\n') ? nl + 1 : 0;
  }
  return false;
}

// Classify a cast target from its ST / USN service string.
static inline const char *cast_kind(const char *st) {
  if (ci_contains(st, "dial-multiscreen")) return "Chromecast/DIAL";
  if (ci_contains(st, "roku"))             return "Roku";
  if (ci_contains(st, "MediaRenderer"))    return "DLNA/UPnP";
  return "unknown cast device";
}
