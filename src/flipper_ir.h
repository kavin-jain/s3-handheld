// Pure Flipper Zero ".ir" record parser — host-testable.
// Lets the firmware consume the CC0-licensed Flipper-IRDB (or a user's own
// Flipper SD) directly, instead of baking a code table in. A .ir file holds one
// or more records:
//   name: Power
//   type: parsed
//   protocol: NEC
//   address: 04 00 00 00
//   command: 08 00 00 00
// or a raw record (type: raw + frequency/data). This parses one record's fields;
// the SD read + IRremoteESP8266 send are bring-up. Reuses ir_kv from ir_codes.h.
#pragma once
#include "ir_codes.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum IrRecType { IRREC_NONE = 0, IRREC_PARSED, IRREC_RAW };

struct FlipperIr {
  char    name[24];
  char    protocol[16];
  uint8_t addr[4];
  uint8_t cmd[4];
  int     addr_len, cmd_len;
  int     type;                    // IrRecType
};

// Parse a Flipper byte field ("04 00 00 00") into out[0..max); returns count.
static inline int flipper_bytes(const char *s, uint8_t *out, int max) {
  int n = 0;
  while (*s && n < max) {
    while (*s == ' ' || *s == '\t') s++;
    int hi = ir_hexval(s[0]);
    if (hi < 0) break;
    int lo = ir_hexval(s[1]);
    if (lo < 0) break;
    out[n++] = (uint8_t)((hi << 4) | lo);
    s += 2;
  }
  return n;
}

static inline int flipper_type(const char *t) {
  if (!strcmp(t, "parsed")) return IRREC_PARSED;
  if (!strcmp(t, "raw"))    return IRREC_RAW;
  return IRREC_NONE;
}

// Parse one record (a block of "key: value" lines) into fp. Returns true if it
// found at least a name + a recognised type.
static inline bool flipper_ir_parse(const char *record, FlipperIr *fp) {
  if (!record || !fp) return false;
  memset(fp, 0, sizeof *fp);
  char v[48];
  for (const char *line = record; line && *line; ) {
    if (ir_kv(line, "name", v, sizeof v)) {
      strncpy(fp->name, v, sizeof fp->name - 1);
    } else if (ir_kv(line, "type", v, sizeof v)) {
      fp->type = flipper_type(v);
    } else if (ir_kv(line, "protocol", v, sizeof v)) {
      strncpy(fp->protocol, v, sizeof fp->protocol - 1);
    } else if (ir_kv(line, "address", v, sizeof v)) {
      fp->addr_len = flipper_bytes(v, fp->addr, 4);
    } else if (ir_kv(line, "command", v, sizeof v)) {
      fp->cmd_len = flipper_bytes(v, fp->cmd, 4);
    }
    const char *nl = line; while (*nl && *nl != '\n') nl++;
    line = (*nl == '\n') ? nl + 1 : 0;
  }
  return fp->name[0] && fp->type != IRREC_NONE;
}
