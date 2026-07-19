// Pure NDEF URI record builder (NFC Forum URI RTD) — host-testable byte encoding.
// The page-write to a tag is hardware (nfc_pn532) and a bring-up step.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Returns the URI-prefix abbreviation code and how many chars it covers.
static inline uint8_t ndef_uri_prefix(const char *url, size_t *skip) {
  static const struct { const char *p; uint8_t code; } t[] = {
    {"https://www.", 0x02}, {"http://www.", 0x01}, {"https://", 0x04}, {"http://", 0x03},
  };
  for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
    size_t l = strlen(t[i].p);
    if (strncmp(url, t[i].p, l) == 0) { *skip = l; return t[i].code; }
  }
  *skip = 0;
  return 0x00;   // no abbreviation
}

// Build an NDEF URI record into out. Returns bytes written, or 0 on overflow/bad args.
static inline size_t ndef_uri_record(const char *url, uint8_t *out, size_t cap) {
  if (!url || !out) return 0;
  size_t skip;
  uint8_t pfx = ndef_uri_prefix(url, &skip);
  size_t urilen = strlen(url) - skip;
  size_t payloadlen = 1 + urilen;          // prefix byte + URI text
  size_t total = 4 + payloadlen;           // header, typelen, payloadlen, 'U' + payload
  if (payloadlen > 255 || total > cap) return 0;
  size_t o = 0;
  out[o++] = 0xD1;                          // MB|ME|SR, TNF=well-known
  out[o++] = 0x01;                          // type length
  out[o++] = (uint8_t)payloadlen;
  out[o++] = 'U';                           // URI record type
  out[o++] = pfx;
  memcpy(out + o, url + skip, urilen);
  o += urilen;
  return o;
}
