// PN532 NFC (13.56 MHz, I2C) — read a card, and dictionary-crack Mifare keys.
// Pure parsing/formatting is in nfc_keys.h (host-tested); this is the hardware side.
#pragma once
#include <stdint.h>
#include <stddef.h>

bool nfc_begin();                                   // init over I2C; true if PN532 answers
bool nfc_present();
bool nfc_read_uid(uint8_t *uid, uint8_t *len);      // poll one card (ISO14443A)

// Authenticate a Mifare Classic block. keyType 0 = key A, 1 = key B.
bool nfc_auth_block(uint8_t *uid, uint8_t uidLen, uint8_t block, uint8_t keyType,
                    const uint8_t key[6]);

// Try every key in an SD dictionary file against (uid, block). Fills outKey + true on hit.
bool nfc_crack_block(uint8_t *uid, uint8_t uidLen, uint8_t block, uint8_t keyType,
                     const char *dictPath, uint8_t outKey[6]);

// Write an NDEF (TLV-wrapped) message to an NTAG2xx tag starting at page 4,
// 4 bytes/page. Requires a tag already tapped (call nfc_read_uid first).
bool nfc_write_ndef(const uint8_t *tlv, size_t len);
