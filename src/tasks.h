// Pure task-line parse for the phone-bridged to-do list — host-testable.
// The phone app syncs tasks as simple markdown-ish lines: "[x] !1 Ship it".
// This parses one line into done / priority / text. The BLE sync is bring-up.
#pragma once

// Parse "[ ]" or "[x]" checkbox, optional " !N" priority (1..3), then text.
// On success sets *done, *prio (0 = none), *text (points into line). false if
// the line isn't a task.
static inline bool task_parse(const char *line, bool *done, int *prio,
                              const char **text) {
  if (!line || line[0] != '[' || line[2] != ']') return false;
  char c = line[1];
  if (c != ' ' && c != 'x' && c != 'X') return false;
  *done = (c == 'x' || c == 'X');
  const char *p = line + 3;
  while (*p == ' ') p++;
  *prio = 0;
  if (*p == '!' && p[1] >= '1' && p[1] <= '3') {
    *prio = p[1] - '0';
    p += 2;
    while (*p == ' ') p++;
  }
  *text = p;
  return true;
}
