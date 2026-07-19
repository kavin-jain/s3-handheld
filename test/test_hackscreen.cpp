// Host unit test for the pure hacker-screen PRNG.
//   g++ -std=c++17 test/test_hackscreen.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/hackscreen.h"
#include <cassert>
#include <cstring>

static bool all_hex(const char *s) {
  for (; *s; s++)
    if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f'))) return false;
  return true;
}

int main() {
  // Same seed -> identical output (reproducible).
  uint32_t a = 0xC0FFEE, b = 0xC0FFEE;
  char la[17], lb[17];
  hack_line(&a, la, 16);
  hack_line(&b, lb, 16);
  assert(strcmp(la, lb) == 0);
  assert(strlen(la) == 16 && all_hex(la));

  // A different seed diverges.
  uint32_t c = 0x12345;
  char lc[17];
  hack_line(&c, lc, 16);
  assert(strcmp(la, lc) != 0);

  // The stream advances (two successive lines from the same state differ).
  char la2[17];
  hack_line(&a, la2, 16);
  assert(strcmp(la, la2) != 0);
  return 0;
}
