// Host unit test for the pure usage-meter formatting.
//   g++ -std=c++17 test/test_usage_fmt.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/usage_fmt.h"
#include <cassert>
#include <cstring>

int main() {
  assert(usage_pct(62, 100) == 62);
  assert(usage_pct(150, 100) == 100);   // clamp high
  assert(usage_pct(-5, 100) == 0);      // clamp low
  assert(usage_pct(5, 0) == 0);         // no div-by-zero

  char b[16];
  assert(fmt_hms(12180, b, sizeof b) && strcmp(b, "3h 23m") == 0);   // 3h23m
  assert(fmt_hms(150, b, sizeof b) && strcmp(b, "2m") == 0);
  assert(fmt_hms(-1, b, sizeof b) && strcmp(b, "0m") == 0);
  return 0;
}
