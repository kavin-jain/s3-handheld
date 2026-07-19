// Pure ASCII -> USB HID usage-code encoder for BadUSB — host-testable.
// The inverse of hid_keymap.h: turns a payload string into HID keyboard reports
// to inject. The USBHIDKeyboard press/release timing is bring-up. This is the
// keymap; it round-trips against hid_to_ascii() in the test.
#pragma once
#include <stdint.h>

// Map an ASCII char to its HID usage code, setting *shift. Returns 0 if unsupported.
static inline uint8_t ascii_to_hid(char c, bool *shift) {
  *shift = false;
  if (c >= 'a' && c <= 'z') return (uint8_t)(0x04 + (c - 'a'));
  if (c >= 'A' && c <= 'Z') { *shift = true; return (uint8_t)(0x04 + (c - 'A')); }
  if (c >= '1' && c <= '9') return (uint8_t)(0x1E + (c - '1'));
  if (c == '0') return 0x27;

  // Shifted number-row symbols !@#$%^&*()
  const char *sym = "!@#$%^&*()";
  for (int i = 0; i < 10; i++)
    if (c == sym[i]) { *shift = true; return (uint8_t)(0x1E + i); }

  switch (c) {
    case ' ':  return 0x2C;
    case '\n': return 0x28;
    case '\b': return 0x2A;
    case '\t': return 0x2B;
    case '-':  return 0x2D;
    case '_':  *shift = true; return 0x2D;
    case '=':  return 0x2E;
    case '+':  *shift = true; return 0x2E;
    case '[':  return 0x2F;
    case '{':  *shift = true; return 0x2F;
    case ']':  return 0x30;
    case '}':  *shift = true; return 0x30;
    case '\\': return 0x31;
    case '|':  *shift = true; return 0x31;
    case ';':  return 0x33;
    case ':':  *shift = true; return 0x33;
    case '\'': return 0x34;
    case '"':  *shift = true; return 0x34;
    case '`':  return 0x35;
    case '~':  *shift = true; return 0x35;
    case ',':  return 0x36;
    case '<':  *shift = true; return 0x36;
    case '.':  return 0x37;
    case '>':  *shift = true; return 0x37;
    case '/':  return 0x38;
    case '?':  *shift = true; return 0x38;
    default:   return 0;
  }
}
