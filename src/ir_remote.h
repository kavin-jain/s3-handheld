// IR learn + blast (IRremoteESP8266) — TX GPIO47, RX GPIO48.
// Flipper-.ir brand-DB parsing lives in ir_codes.h (host-tested).
#pragma once
#include <stdint.h>
#include "flipper_ir.h"

bool ir_begin();
// Capture one IR frame within timeout_ms. Fills protocol/value/bits; true on a hit.
bool ir_learn(uint16_t timeout_ms, uint8_t *proto, uint64_t *value, uint16_t *bits);
void ir_send_nec(uint8_t addr, uint8_t cmd);              // send a NEC address/command
bool ir_send(uint8_t proto, uint64_t value, uint16_t bits);  // re-blast a learned frame

// Send a parsed record loaded from a Flipper .ir file (CC0 Flipper-IRDB or the
// user's own SD). Maps NEC/NECext/Samsung32/SIRC*/RC6 to IRremoteESP8266's
// encoders; returns false for raw / unsupported protocols. Bring-up.
bool ir_send_flipper(const FlipperIr *fp);
