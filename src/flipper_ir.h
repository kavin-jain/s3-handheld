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

// Is this line the "name:" field that starts a record?
static inline bool flipper_is_name_line(const char *line) {
  char v[8];
  return ir_kv(line, "name", v, sizeof v);
}

// Parse one record starting at `record` into fp. Stops at the next "name:" line
// so it's safe to call on the whole file positioned at a record start. Returns
// true if it found at least a name + a recognised type.
static inline bool flipper_ir_parse(const char *record, FlipperIr *fp) {
  if (!record || !fp) return false;
  memset(fp, 0, sizeof *fp);
  char v[48];
  bool got_name = false;
  for (const char *line = record; line && *line; ) {
    if (ir_kv(line, "name", v, sizeof v)) {
      if (got_name) break;                    // next record begins here — stop
      strncpy(fp->name, v, sizeof fp->name - 1);
      got_name = true;
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

// Number of records (button entries) in a whole .ir file's text.
static inline int flipper_ir_count(const char *text) {
  int n = 0;
  for (const char *line = text; line && *line; ) {
    if (flipper_is_name_line(line)) n++;
    const char *nl = line; while (*nl && *nl != '\n') nl++;
    line = (*nl == '\n') ? nl + 1 : 0;
  }
  return n;
}

// Parse the idx-th record of a whole .ir file into fp. false if idx is out of range.
static inline bool flipper_ir_at(const char *text, int idx, FlipperIr *fp) {
  int seen = 0;
  for (const char *line = text; line && *line; ) {
    if (flipper_is_name_line(line)) {
      if (seen == idx) return flipper_ir_parse(line, fp);
      seen++;
    }
    const char *nl = line; while (*nl && *nl != '\n') nl++;
    line = (*nl == '\n') ? nl + 1 : 0;
  }
  return false;
}
