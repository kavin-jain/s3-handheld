// Host unit test for the pure A/C state helpers.
//   g++ -std=c++17 test/test_ac_state.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ac_state.h"
#include <cassert>
#include <cstring>

int main() {
  // Temperature clamp to 16..30.
  assert(ac_clamp_temp(10) == 16);
  assert(ac_clamp_temp(35) == 30);
  assert(ac_clamp_temp(24) == 24);
  assert(ac_clamp_temp(16) == 16 && ac_clamp_temp(30) == 30);

  // Mode + fan labels.
  assert(strcmp(ac_mode_name(AC_COOL), "Cool") == 0);
  assert(strcmp(ac_mode_name(AC_AUTO), "Auto") == 0);
  assert(strcmp(ac_mode_name(AC_MODE_N), "?") == 0);
  assert(strcmp(ac_fan_name(AC_FAN_HIGH), "High") == 0);
  assert(strcmp(ac_fan_name(AC_FAN_AUTO), "Auto") == 0);
  assert(strcmp(ac_fan_name(99), "?") == 0);
  return 0;
}
