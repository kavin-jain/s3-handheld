// Pure value-editor stepping/clamping — host-testable.
#pragma once

// Apply an encoder delta (in detents) to a value, stepping by `step` and clamping.
static inline int edit_apply(int val, int delta, int lo, int hi, int step) {
  int v = val + delta * step;
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return v;
}
