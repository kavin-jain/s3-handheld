// Pure EMV contactless "public data" decode — host-testable.
// A contactless card exposes Track-2-Equivalent Data (tag 0x57): the PAN,
// expiry and service code — the same data printed on the card face. No CVV, no
// PIN, no cryptogram keys. The PN532 APDU exchange (SELECT PPSE -> READ RECORD)
// is bring-up; this file parses tag 57 and sanity-checks it.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Luhn (mod-10) check digit validation over an ASCII digit string.
static inline bool luhn_valid(const char *d) {
  int sum = 0, alt = 0;
  size_t n = 0; while (d[n]) n++;
  if (n == 0) return false;
  for (int i = (int)n - 1; i >= 0; i--) {
    if (d[i] < '0' || d[i] > '9') return false;
    int x = d[i] - '0';
    if (alt) { x *= 2; if (x > 9) x -= 9; }
    sum += x; alt = !alt;
  }
  return sum % 10 == 0;
}

// Card network from the PAN's leading digits (IIN/BIN ranges).
static inline const char *card_network(const char *pan) {
  if (!pan || !pan[0] || !pan[1]) return "Unknown";
  if (pan[0] == '4') return "Visa";
  if (pan[0] == '3' && (pan[1] == '4' || pan[1] == '7')) return "Amex";
  int p2 = (pan[0] - '0') * 10 + (pan[1] - '0');
  if (p2 >= 51 && p2 <= 55) return "Mastercard";
  if (pan[0] == '6') return "Discover/RuPay";
  return "Unknown";
}

// Parse nibble-packed Track-2-Equivalent (tag 57). Layout: PAN digits, 0xD field
// separator, YYMM expiry, then service code / discretionary, 0xF padded. Fills
// pan (NUL-terminated) and yymm[5]; true if a plausible PAN + full expiry found.
static inline bool emv_parse_track2(const uint8_t *t2, int len,
                                    char *pan, size_t pan_cap, char yymm[5]) {
  if (!t2 || !pan || !yymm || pan_cap < 2 || len <= 0) return false;
  size_t pi = 0; int state = 0, exp = 0;
  for (int i = 0; i < len * 2; i++) {
    uint8_t nib = (i & 1) ? (t2[i / 2] & 0x0F) : (t2[i / 2] >> 4);
    if (state == 0) {                       // PAN
      if (nib == 0xD) { state = 1; continue; }
      if (nib > 9) break;                   // 0xF pad / malformed -> stop
      if (pi < pan_cap - 1) pan[pi++] = (char)('0' + nib);
    } else {                                // expiry YYMM
      if (nib > 9) break;
      yymm[exp++] = (char)('0' + nib);
      if (exp == 4) break;
    }
  }
  pan[pi] = 0; yymm[exp] = 0;
  return pi >= 12 && exp == 4;
}
