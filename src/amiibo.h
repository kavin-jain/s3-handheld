// Pure amiibo / NTAG215 dump helpers — host-testable.
// An amiibo is a locked NTAG215 (540 bytes). The PN532 read/write of the full
// dump is bring-up; this file identifies the tag, validates its UID checksums,
// and pulls out the 8-byte figure id every amiibo database keys on.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NTAG215_SIZE    540
#define NTAG215_CC_OFF  12      // page 3: capability container
#define AMIIBO_ID_OFF   0x54    // page 21: 8-byte figure/model id

// ISO14443-3 cascade Block Check Characters for a 7-byte UID.
static inline uint8_t nfc_bcc0(const uint8_t uid[7]) {
  return (uint8_t)(0x88 ^ uid[0] ^ uid[1] ^ uid[2]);   // 0x88 = cascade tag
}
static inline uint8_t nfc_bcc1(const uint8_t uid[7]) {
  return (uint8_t)(uid[3] ^ uid[4] ^ uid[5] ^ uid[6]);
}

// True if the dump carries the NTAG215 capability container (E1 10 3E 00) —
// the tag type every amiibo uses.
static inline bool ntag215_is_amiibo(const uint8_t *d, size_t n) {
  if (!d || n < NTAG215_SIZE) return false;
  return d[NTAG215_CC_OFF] == 0xE1 && d[NTAG215_CC_OFF + 1] == 0x10 &&
         d[NTAG215_CC_OFF + 2] == 0x3E && d[NTAG215_CC_OFF + 3] == 0x00;
}

// Validate the stored UID's BCC bytes. Dump layout: uid0-2,bcc0 | uid3-6 | bcc1.
static inline bool ntag215_uid_valid(const uint8_t *d, size_t n) {
  if (!d || n < NTAG215_SIZE) return false;
  const uint8_t uid[7] = {d[0], d[1], d[2], d[4], d[5], d[6], d[7]};
  return d[3] == nfc_bcc0(uid) && d[8] == nfc_bcc1(uid);
}

// Extract the 16-hex-digit amiibo figure id (8 bytes at page 21). out needs 17.
static inline void amiibo_id_hex(const uint8_t *d, char out[17]) {
  for (int i = 0; i < 8; i++)
    snprintf(out + i * 2, 3, "%02X", d[AMIIBO_ID_OFF + i]);
}
