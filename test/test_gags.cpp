// Host unit test: every gag payload is valid DuckyScript (cross-checks the parser).
//   g++ -std=c++17 test/test_gags.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/gags.h"
#include <cassert>
#include <cstring>

int main() {
  assert(GAG_COUNT >= 5);
  char arg[128];
  for (int i = 0; i < GAG_COUNT; i++) {
    int cmd = ducky_parse(GAGS[i].line, arg, sizeof arg);
    assert(cmd == GAGS[i].expect);       // library entry matches the parser
    assert(GAGS[i].name && GAGS[i].name[0]);
  }
  // Spot-check an argument is extracted.
  ducky_parse(GAGS[0].line, arg, sizeof arg);
  assert(strcmp(arg, "https://youtu.be/dQw4w9WgXcQ") == 0);
  return 0;
}
