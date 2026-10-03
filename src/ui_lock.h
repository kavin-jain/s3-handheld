#pragma once
// Pure PIN-entry state machine — host-testable, no LVGL/Arduino dependency.
// main.cpp's build_lock() screen drives this via the existing encoder edit-mode
// (g_edit_val) for dialling each digit and the existing g_action_cb (ACTION
// button) to confirm one and advance — no new input plumbing, same idiom as
// the brightness/intensity edit screens.
#include <stdint.h>

struct LockState {
  int pin;       // stored PIN, -1 = none configured
  int entry[4];  // digits entered so far this attempt
  int pos;       // 0..4; 4 = all digits entered, ready to check
};

// A configured PIN is always 0-9999 (the only range 4 real keypad digits can
// ever produce, via lock_pin_from_digits). Enforced here, not at each call site,
// so a corrupt/out-of-range value loaded from anywhere can never be treated as
// "configured" -- it would otherwise be an unmatchable PIN, i.e. a permanent
// self-lockout. See src/config.h's cfg_parse (no clamp there -- parsing is meant
// to be faithful) and main.cpp's setup() restore block (resets an out-of-range
// loaded value to -1, loud in the serial log, rather than guessing a "real" PIN).
static inline bool lock_configured(const LockState *s) {
  return s->pin >= 0 && s->pin <= 9999;
}

static inline void lock_reset_entry(LockState *s) {
  s->pos = 0;
  for (int i = 0; i < 4; i++) s->entry[i] = 0;
}

// Records `digit` (0-9) at the current position and advances. No-op past pos 4.
static inline void lock_confirm_digit(LockState *s, int digit) {
  if (s->pos >= 4) return;
  s->entry[s->pos++] = digit;
}

static inline bool lock_entry_complete(const LockState *s) { return s->pos >= 4; }

static inline int lock_pin_from_digits(int d0, int d1, int d2, int d3) {
  return d0 * 1000 + d1 * 100 + d2 * 10 + d3;
}

// Compares the 4 entered digits against the stored PIN.
static inline bool lock_entry_matches(const LockState *s) {
  return s->pin == lock_pin_from_digits(s->entry[0], s->entry[1], s->entry[2], s->entry[3]);
}
