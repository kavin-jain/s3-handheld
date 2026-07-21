#include "ir_remote.h"
#include "ac_db.h"
#include "pins.h"
#include <Arduino.h>
#include <IRsend.h>
#include <IRrecv.h>

// Guard: the AC-brand db (ac_db.h) hardcodes decode_type_t values so it can stay
// pure/host-testable. Verify they still match the installed IRremoteESP8266 enum
// — a library bump that renumbers protocols fails the build here, loudly.
static_assert((int)DAIKIN     == 16, "ac_db.h out of sync with IRremoteESP8266 (DAIKIN)");
static_assert((int)GREE       == 24, "ac_db.h out of sync (GREE)");
static_assert((int)HITACHI_AC == 40, "ac_db.h out of sync (HITACHI_AC)");
static_assert((int)LG2        == 51, "ac_db.h out of sync (LG2)");
static_assert((int)VOLTAS     == 90, "ac_db.h out of sync (VOLTAS)");
static_assert((int)SONY       == 4,  "irdb.h out of sync (SONY)");
static_assert((int)SAMSUNG    == 7,  "irdb.h out of sync (SAMSUNG)");

// Adafruit 5639 emitter has its own FET — GPIO47 drives Signal directly.
static IRsend irsend(PIN_IR_TX);
static IRrecv irrecv(PIN_IR_RX);
static bool s_ready = false;

bool ir_begin() {
  irsend.begin();
  irrecv.enableIRIn(true);   // pullup: some TSOP-compatible clones float when idle
  s_ready = true;
  return true;
}

bool ir_learn(uint16_t timeout_ms, uint8_t *proto, uint64_t *value, uint16_t *bits) {
  if (!s_ready) return false;
  decode_results r;
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    if (irrecv.decode(&r)) {
      if (proto) *proto = (uint8_t)r.decode_type;
      if (value) *value = r.value;
      if (bits)  *bits  = r.bits;
      irrecv.resume();
      return true;
    }
    delay(5);
  }
  return false;
}

void ir_send_nec(uint8_t addr, uint8_t cmd) {
  if (s_ready) irsend.sendNEC(irsend.encodeNEC(addr, cmd));
}

bool ir_send(uint8_t proto, uint64_t value, uint16_t bits) {
  if (!s_ready) return false;
  return irsend.send((decode_type_t)proto, value, bits);
}

// Send a Flipper .ir "parsed" record. Flipper stores address+command as little-
// endian bytes; each protocol has its own encoder in IRremoteESP8266. Bring-up.
bool ir_send_flipper(const FlipperIr *fp) {
  if (!s_ready || !fp || fp->type != IRREC_PARSED) return false;
  uint16_t a16 = (uint16_t)(fp->addr[0] | (fp->addr[1] << 8));
  uint16_t c16 = (uint16_t)(fp->cmd[0]  | (fp->cmd[1]  << 8));
  const char *p = fp->protocol;
  if (!strcmp(p, "NEC") || !strcmp(p, "NECext") || !strcmp(p, "NEC42")) {
    irsend.sendNEC(irsend.encodeNEC(a16, c16));
    return true;
  }
  if (!strcmp(p, "Samsung32")) {
    irsend.sendSAMSUNG(irsend.encodeSAMSUNG(fp->addr[0], fp->cmd[0]), 32);
    return true;
  }
  if (!strcmp(p, "SIRC") || !strcmp(p, "SIRC15") || !strcmp(p, "SIRC20")) {
    uint16_t bits = !strcmp(p, "SIRC15") ? 15 : !strcmp(p, "SIRC20") ? 20 : 12;
    irsend.sendSony(irsend.encodeSony(bits, fp->cmd[0], a16), bits);
    return true;
  }
  if (!strcmp(p, "RC6")) {
    irsend.sendRC6(irsend.encodeRC6(a16, fp->cmd[0]));
    return true;
  }
  return false;    // raw / RC5 / Kaseikyo etc. — not yet mapped
}
