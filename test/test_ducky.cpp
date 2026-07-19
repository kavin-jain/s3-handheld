// Host unit test for the pure DuckyScript parser.
//   g++ -std=c++17 test/test_ducky.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ducky.h"
#include <cassert>
#include <cstring>

int main() {
  char a[64];
  assert(ducky_parse("STRING hello world", a, sizeof a) == DK_STRING &&
         strcmp(a, "hello world") == 0);
  assert(ducky_parse("REM just a comment", a, sizeof a) == DK_REM);
  assert(ducky_parse("DELAY 500", a, sizeof a) == DK_DELAY && strcmp(a, "500") == 0);
  assert(ducky_parse("ENTER", a, sizeof a) == DK_ENTER);
  assert(ducky_parse("GUI r", a, sizeof a) == DK_GUI && strcmp(a, "r") == 0);
  assert(ducky_parse("WINDOWS d", a, sizeof a) == DK_GUI && strcmp(a, "d") == 0);
  assert(ducky_parse("", a, sizeof a) == DK_NONE);
  assert(ducky_parse("   ", a, sizeof a) == DK_NONE);
  assert(ducky_parse("CTRL ALT DELETE", a, sizeof a) == DK_KEYCOMBO);
  assert(ducky_parse("STRINGX foo", a, sizeof a) == DK_KEYCOMBO);  // boundary: not STRING
  return 0;
}
