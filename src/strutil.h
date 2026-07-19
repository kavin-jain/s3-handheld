// Tiny shared string helpers (pure, host-testable).
#pragma once
#include <stddef.h>

static inline char str_lc(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

// Does `s` end with `suffix`? (e.g. filename ends with ".ir"). Case-sensitive.
static inline bool str_ends_with(const char *s, const char *suffix) {
  if (!s || !suffix) return false;
  size_t ls = 0, lf = 0;
  while (s[ls]) ls++;
  while (suffix[lf]) lf++;
  if (lf > ls) return false;
  for (size_t i = 0; i < lf; i++)
    if (s[ls - lf + i] != suffix[i]) return false;
  return true;
}

// Case-insensitive substring test. Safe on NUL-terminated strings.
static inline bool ci_contains(const char *hay, const char *needle) {
  if (!hay || !needle) return false;
  for (size_t i = 0; hay[i]; i++) {
    size_t j = 0;
    while (needle[j] && str_lc(hay[i + j]) == str_lc(needle[j])) j++;
    if (!needle[j]) return true;
  }
  return false;
}
