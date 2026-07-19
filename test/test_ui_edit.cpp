// Host unit test for the pure value-editor stepping.
//   g++ -std=c++17 test/test_ui_edit.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ui_edit.h"
#include <cassert>

int main() {
  assert(edit_apply(50, 1, 0, 100, 10) == 60);
  assert(edit_apply(50, -1, 0, 100, 10) == 40);
  assert(edit_apply(95, 1, 0, 100, 10) == 100);   // clamp high
  assert(edit_apply(5, -1, 0, 100, 10) == 0);      // clamp low
  assert(edit_apply(50, 3, 0, 100, 10) == 80);     // multi-step
  assert(edit_apply(10, -5, 10, 100, 10) == 10);   // clamp at lo
  return 0;
}
