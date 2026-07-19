// Pure formatting for the Claude usage meter — host-testable.
#pragma once
#include <stddef.h>
#include <stdio.h>

static inline int usage_pct(int used, int total) {
  if (total <= 0) return 0;
  int p = (int)((long)used * 100 / total);
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

// Seconds -> "3h 23m" (or "2m" under an hour). Returns chars written, 0 on overflow.
static inline size_t fmt_hms(int secs, char *out, size_t cap) {
  if (!out || cap == 0) return 0;
  out[0] = 0;
  if (secs < 0) secs = 0;
  int h = secs / 3600, m = (secs % 3600) / 60;
  int n = h > 0 ? snprintf(out, cap, "%dh %02dm", h, m)
                : snprintf(out, cap, "%dm", m);
  if (n < 0 || (size_t)n >= cap) { out[0] = 0; return 0; }
  return (size_t)n;
}
