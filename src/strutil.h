// Tiny shared string helpers (pure, host-testable).
#pragma once
#include <stddef.h>

static inline char str_lc(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

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
