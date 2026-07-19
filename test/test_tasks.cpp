// Host unit test for the pure task-line parse.
//   g++ -std=c++17 test/test_tasks.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/tasks.h"
#include <cassert>
#include <cstring>

int main() {
  bool done; int prio; const char *text;

  assert(task_parse("[x] !1 Ship firmware", &done, &prio, &text));
  assert(done && prio == 1 && strcmp(text, "Ship firmware") == 0);

  assert(task_parse("[ ] Buy milk", &done, &prio, &text));
  assert(!done && prio == 0 && strcmp(text, "Buy milk") == 0);

  assert(task_parse("[ ] !3 Call bank", &done, &prio, &text));
  assert(!done && prio == 3 && strcmp(text, "Call bank") == 0);

  // Empty task text is valid.
  assert(task_parse("[ ]", &done, &prio, &text));
  assert(!done && prio == 0 && text[0] == 0);

  // Priority out of range is treated as plain text, not a priority.
  assert(task_parse("[ ] !9 weird", &done, &prio, &text));
  assert(prio == 0 && strcmp(text, "!9 weird") == 0);

  // Not tasks.
  assert(!task_parse("hello world", &done, &prio, &text));
  assert(!task_parse("[z] bad", &done, &prio, &text));
  assert(!task_parse("", &done, &prio, &text));
  return 0;
}
