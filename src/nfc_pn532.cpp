#include "nfc_pn532.h"
#include "nfc_keys.h"
#include "pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_PN532.h>
#include <string.h>

// I2C constructor needs irq/reset pins; ours point at the unused LED pin (PN532
// IRQ/RST are open on this board — see pins.h / BRINGUP). We poll, not IRQ.
static Adafruit_PN532 nfc(PIN_PN532_IRQ, PIN_PN532_RST, &Wire);
static bool s_present = false;

bool nfc_begin() {
  nfc.begin();                                   // shares the already-started Wire bus
  uint32_t v = nfc.getFirmwareVersion();
  s_present = (v != 0);
  if (s_present) nfc.SAMConfig();
  return s_present;
}

bool nfc_present() { return s_present; }

bool nfc_read_uid(uint8_t *uid, uint8_t *len) {
  if (!s_present || !uid || !len) return false;
  return nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, len, 60);  // 60 ms poll
}

bool nfc_auth_block(uint8_t *uid, uint8_t uidLen, uint8_t block, uint8_t keyType,
                    const uint8_t key[6]) {
  if (!s_present) return false;
  return nfc.mifareclassic_AuthenticateBlock(uid, uidLen, block, keyType,
                                             (uint8_t *)key) == 1;
}

bool nfc_write_ndef(const uint8_t *tlv, size_t len) {
  if (!s_present || !tlv) return false;
  uint8_t page[4];
  for (size_t off = 0, pg = 4; off < len; off += 4, pg++) {
    memset(page, 0, sizeof page);
    size_t n = (len - off) < 4 ? (len - off) : 4;
    memcpy(page, tlv + off, n);
    if (nfc.ntag2xx_WritePage((uint8_t)pg, page) != 1) return false;
  }
  return true;
}

bool nfc_crack_block(uint8_t *uid, uint8_t uidLen, uint8_t block, uint8_t keyType,
                     const char *dictPath, uint8_t outKey[6]) {
  if (!s_present || !dictPath || !outKey) return false;
  File f = SD.open(dictPath);
  if (!f) return false;
  char line[24];
  uint8_t key[6];
  bool hit = false;
  while (f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = 0;
    if (!nfc_parse_key(line, key)) continue;
    if (nfc_auth_block(uid, uidLen, block, keyType, key)) {
      memcpy(outKey, key, 6);
      hit = true;
      break;
    }
  }
  f.close();
  return hit;
}
