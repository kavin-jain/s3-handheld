// Pure DuckyScript line parser — no Arduino/USB, so it host-unit-tests.
#pragma once
#include <stddef.h>

enum DuckyCmd { DK_NONE, DK_REM, DK_STRING, DK_DELAY, DK_ENTER, DK_GUI, DK_KEYCOMBO };

// True if s begins with keyword kw at a word boundary; *rest -> text after it.
static inline bool ducky_starts(const char *s, const char *kw, const char **rest) {
  size_t k = 0;
  while (kw[k]) { if (s[k] != kw[k]) return false; k++; }
  char b = s[k];
  if (b != ' ' && b != '\0' && b != '\r' && b != '\n') return false;   // boundary
  const char *r = s + k;
  while (*r == ' ') r++;
  if (rest) *rest = r;
  return true;
}

// Parse one DuckyScript line into a command + its argument (copied into arg).
static inline int ducky_parse(const char *line, char *arg, size_t cap) {
  if (arg && cap) arg[0] = 0;
  if (!line) return DK_NONE;
  while (*line == ' ' || *line == '\t') line++;
  if (*line == '\0' || *line == '\r' || *line == '\n') return DK_NONE;

  const char *rest = line;
  int cmd;
  if      (ducky_starts(line, "REM", &rest))     cmd = DK_REM;
  else if (ducky_starts(line, "STRING", &rest))  cmd = DK_STRING;
  else if (ducky_starts(line, "DELAY", &rest))   cmd = DK_DELAY;
  else if (ducky_starts(line, "ENTER", &rest))   cmd = DK_ENTER;
  else if (ducky_starts(line, "GUI", &rest) ||
           ducky_starts(line, "WINDOWS", &rest)) cmd = DK_GUI;
  else { cmd = DK_KEYCOMBO; rest = line; }        // e.g. "CTRL ALT DELETE"

  if (arg && cap) {
    size_t o = 0;
    while (rest[o] && rest[o] != '\r' && rest[o] != '\n' && o < cap - 1) { arg[o] = rest[o]; o++; }
    arg[o] = 0;
  }
  return cmd;
}
