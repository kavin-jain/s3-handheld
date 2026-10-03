// Pure NFC helpers — Mifare key-dictionary line parsing + UID formatting.
// No Arduino/PN532 here, so it host-unit-tests (test/test_nfc_keys.cpp).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static inline int nfc_hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Parse one dictionary line ("A0A1A2A3A4A5") into a 6-byte key.
// Skips comments (#) and blank lines. Rejects wrong-length / non-hex. -> true on a key.
static inline bool nfc_parse_key(const char *line, uint8_t out[6]) {
  if (!line || !out) return false;
  while (*line == ' ' || *line == '\t') line++;
  if (*line == '#' || *line == '\0' || *line == '\n' || *line == '\r') return false;
  for (int i = 0; i < 6; i++) {
    // Check hi before reading line[i*2+1]: line is NUL-terminated, so once
    // line[i*2] is confirmed non-NUL the next byte is guaranteed to exist
    // (more content or the eventual NUL) -- reading both unconditionally
    // first (as this used to) can read one byte past a buffer whose NUL
    // lands exactly on an even offset relative to `line`.
    int hi = nfc_hexval(line[i * 2]);
    if (hi < 0) return false;
    int lo = nfc_hexval(line[i * 2 + 1]);
    if (lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  if (nfc_hexval(line[12]) >= 0) return false;   // 13th hex char -> too long, reject
  return true;
}

// Format a UID as "04:A2:1B". Returns chars written, or 0 on overflow/bad args.
static inline size_t nfc_uid_hex(const uint8_t *uid, uint8_t len, char *out, size_t cap) {
  if (!out || cap == 0) return 0;
  out[0] = 0;
  if (!uid) return 0;
  size_t pos = 0;
  for (uint8_t i = 0; i < len; i++) {
    int n = snprintf(out + pos, cap - pos, i ? ":%02X" : "%02X", uid[i]);
    if (n < 0 || (size_t)n >= cap - pos) { out[0] = 0; return 0; }
    pos += (size_t)n;
  }
  return pos;
}
