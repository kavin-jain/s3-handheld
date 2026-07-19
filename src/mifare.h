// Pure MIFARE Classic default-key dictionary — host-testable.
// A dictionary attack tries well-known factory keys against each sector. This
// file holds the key list and names a matched key. The PN532 authenticate loop
// is bring-up. For auditing your own cards only.
#pragma once
#include "nfc_keys.h"     // nfc_parse_key: "FFFFFFFFFFFF" -> 6 bytes
#include <stdint.h>

struct MifareKey { const char *hex; const char *name; };

// The keys that unlock the vast majority of never-reconfigured cards.
static const MifareKey MIFARE_DEFAULT_KEYS[] = {
  {"FFFFFFFFFFFF", "factory default"},
  {"A0A1A2A3A4A5", "MAD key A"},
  {"D3F7D3F7D3F7", "NDEF"},
  {"000000000000", "all-zero"},
  {"B0B1B2B3B4B5", "common B"},
  {"AABBCCDDEEFF", "common"},
  {"4D3A99C351DD", "transport"},
  {"1A982C7E459A", "transport"},
};
static const int MIFARE_KEY_COUNT =
    sizeof(MIFARE_DEFAULT_KEYS) / sizeof(MIFARE_DEFAULT_KEYS[0]);

// Count valid 6-byte keys in a keys.dic text (one hex key per line, # comments
// and blanks skipped, wrong-length rejected). Lets the user drop a big community
// dictionary on SD instead of us hardcoding/guessing keys. Uses nfc_parse_key.
static inline int mifare_dict_count(const char *text) {
  if (!text) return 0;
  int n = 0; uint8_t k[6];
  for (const char *line = text; line && *line; ) {
    if (nfc_parse_key(line, k)) n++;
    const char *nl = line; while (*nl && *nl != '\n') nl++;
    line = (*nl == '\n') ? nl + 1 : 0;
  }
  return n;
}

// Name of a 6-byte key if it's a known default, else "unknown".
static inline const char *mifare_key_name(const uint8_t key[6]) {
  for (int i = 0; i < MIFARE_KEY_COUNT; i++) {
    uint8_t k[6];
    if (!nfc_parse_key(MIFARE_DEFAULT_KEYS[i].hex, k)) continue;
    bool eq = true;
    for (int j = 0; j < 6; j++) if (k[j] != key[j]) { eq = false; break; }
    if (eq) return MIFARE_DEFAULT_KEYS[i].name;
  }
  return "unknown";
}
