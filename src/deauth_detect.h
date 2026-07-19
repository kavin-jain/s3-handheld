// Pure 802.11 deauth/disassoc frame check — host-testable.
// Frame-control byte 0 = subtype(4) | type(2) | version(2). A deauth attack
// floods management frames: deauth subtype 1100 -> 0xC0, disassoc 1010 -> 0xA0.
#pragma once
#include <stdint.h>
#include <stddef.h>

static inline bool is_deauth(const uint8_t *frame, size_t len) {
  if (!frame || len < 1) return false;
  uint8_t fc = frame[0];
  return fc == 0xC0 || fc == 0xA0;   // deauth or disassoc
}

// Hardware side (deauth_detect.cpp): promiscuous-mode monitor.
void     deauth_begin();
bool     deauth_active();
uint32_t deauth_count();
