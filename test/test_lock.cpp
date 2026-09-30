// Host unit test for the pure PIN-entry state machine.
//   g++ -std=c++17 test/test_lock.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ui_lock.h"
#include <cassert>

int main() {
  LockState s = {-1, {0, 0, 0, 0}, 0};
  assert(!lock_configured(&s));

  s.pin = 4269;
  assert(lock_configured(&s));

  // Correct entry matches.
  lock_reset_entry(&s);
  int digits[4] = {4, 2, 6, 9};
  for (int i = 0; i < 4; i++) lock_confirm_digit(&s, digits[i]);
  assert(lock_entry_complete(&s));
  assert(lock_entry_matches(&s));

  // Wrong entry doesn't.
  lock_reset_entry(&s);
  assert(!lock_entry_complete(&s));
  int wrong[4] = {1, 2, 3, 4};
  for (int i = 0; i < 4; i++) lock_confirm_digit(&s, wrong[i]);
  assert(lock_entry_complete(&s));
  assert(!lock_entry_matches(&s));

  // Confirm is a no-op once 4 digits are in (can't overflow the array).
  lock_confirm_digit(&s, 7);
  assert(s.pos == 4);

  assert(lock_pin_from_digits(0, 0, 0, 0) == 0);
  assert(lock_pin_from_digits(9, 9, 9, 9) == 9999);
  return 0;
}
