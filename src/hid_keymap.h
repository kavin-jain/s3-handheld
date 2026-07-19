// Pure USB HID usage-code -> ASCII (US layout) — host-testable.
// A sniffed 2.4 GHz keyboard sends HID keyboard reports (a modifier byte + key
// usage codes). This turns them back into readable text. The nRF24 capture is
// bring-up; this is the keymap. Complements mousejack.h (the inject side).
#pragma once
#include <stdint.h>

// Shift held? modifier bit1 = Left Shift, bit5 = Right Shift.
static inline bool hid_shift(uint8_t mod) { return (mod & 0x02) || (mod & 0x20); }

// Map a HID usage code to a printable char (0 if not printable/unmapped).
static inline char hid_to_ascii(uint8_t usage, bool shift) {
  if (usage >= 0x04 && usage <= 0x1D) {           // a..z
    char base = (char)('a' + (usage - 0x04));
    return shift ? (char)(base - 32) : base;
  }
  if (usage >= 0x1E && usage <= 0x27) {           // 1..9,0
    static const char num[] = "1234567890";
    static const char sym[] = "!@#$%^&*()";
    int i = usage - 0x1E;
    return shift ? sym[i] : num[i];
  }
  switch (usage) {
    case 0x2C: return ' ';
    case 0x28: return '\n';                        // enter
    case 0x2A: return '\b';                        // backspace
    case 0x2B: return '\t';                        // tab
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '[';
    case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x35: return shift ? '~' : '`';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default:   return 0;
  }
}
