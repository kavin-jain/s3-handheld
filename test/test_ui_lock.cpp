// Host unit test for the PIN-entry state machine.
//   g++ -std=c++17 test/test_ui_lock.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ui_lock.h"
#include <cassert>
#include <initializer_list>

int main() {
  // lock_configured's range invariant: this is the single check that stops an
  // out-of-range pin (e.g. loaded from a corrupt config.txt) from ever being
  // treated as a real, enforced lock -- see main.cpp setup()'s restore block.
  LockState s{};
  for (int bad : {-2, -100, 10000, 10001, 99999, 2147483647}) {
    s.pin = bad;
    assert(!lock_configured(&s));
  }
  s.pin = -1;   assert(!lock_configured(&s));   // explicit "no lock"
  s.pin = 0;    assert(lock_configured(&s));
  s.pin = 9999; assert(lock_configured(&s));
  s.pin = 4269; assert(lock_configured(&s));

  // lock_pin_from_digits: the only values 4 real keypad digits can produce.
  assert(lock_pin_from_digits(0, 0, 0, 0) == 0);
  assert(lock_pin_from_digits(9, 9, 9, 9) == 9999);
  assert(lock_pin_from_digits(4, 2, 6, 9) == 4269);

  // Digit entry / reset.
  lock_reset_entry(&s);
  assert(s.pos == 0 && !lock_entry_complete(&s));
  lock_confirm_digit(&s, 4);
  lock_confirm_digit(&s, 2);
  lock_confirm_digit(&s, 6);
  assert(!lock_entry_complete(&s));
  lock_confirm_digit(&s, 9);
  assert(lock_entry_complete(&s));
  lock_confirm_digit(&s, 5);                    // no-op past pos 4
  assert(s.pos == 4);

  // Match / mismatch.
  s.pin = 4269;
  assert(lock_entry_matches(&s));
  s.pin = 4270;
  assert(!lock_entry_matches(&s));

  // The actual bug this test exists to catch: an out-of-range stored pin can
  // never match any real 4-digit entry, by construction of
  // lock_pin_from_digits's range -- but lock_configured must say "not
  // configured" for it, not leave the device permanently locked against an
  // unmatchable value.
  s.pin = 15000;
  lock_reset_entry(&s);
  for (int d : {1, 5, 0, 0}) lock_confirm_digit(&s, d);   // best guess at "15000"
  assert(!lock_entry_matches(&s));      // confirms it's truly unmatchable
  assert(!lock_configured(&s));         // and confirms it's not treated as set
  return 0;
}
