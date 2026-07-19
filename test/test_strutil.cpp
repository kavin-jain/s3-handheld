// Host unit test for the shared string helpers.
//   g++ -std=c++17 test/test_strutil.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/strutil.h"
#include <cassert>

int main() {
  // str_ends_with
  assert(str_ends_with("Samsung.ir", ".ir"));
  assert(str_ends_with("power.IR", ".IR"));
  assert(!str_ends_with("Samsung.ir", ".sub"));
  assert(!str_ends_with("ir", ".ir"));            // suffix longer than string
  assert(str_ends_with("x.ir", ".ir"));
  assert(!str_ends_with("notes.txt", ".ir"));
  assert(!str_ends_with(nullptr, ".ir"));

  // ci_contains (existing helper, keep covered)
  assert(ci_contains("HC-05 module", "hc-05"));
  assert(!ci_contains("AirPods", "hc-05"));
  return 0;
}
