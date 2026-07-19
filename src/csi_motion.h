// Pure WiFi-CSI motion detector — host-testable.
// "See through wall" = watch Channel State Information (per-subcarrier I/Q) from
// ambient WiFi; a moving body perturbs the multipath, so the amplitude *variance*
// over a short window jumps. This file is that amplitude+variance+threshold math.
// The esp_wifi CSI callback that fills the sample window is bring-up.
#pragma once
#include <stddef.h>
#include <math.h>

// Subcarrier amplitude from a raw I/Q pair.
static inline float csi_amplitude(int i, int q) {
  return sqrtf((float)i * i + (float)q * q);
}

static inline float csi_mean(const float *s, int n) {
  if (n <= 0) return 0.f;
  float sum = 0.f;
  for (int k = 0; k < n; k++) sum += s[k];
  return sum / n;
}

// Population variance of the window — the motion signal.
static inline float csi_variance(const float *s, int n) {
  if (n <= 0) return 0.f;
  float m = csi_mean(s, n), acc = 0.f;
  for (int k = 0; k < n; k++) { float d = s[k] - m; acc += d * d; }
  return acc / n;
}

// Motion present if the window variance exceeds threshold (tune per environment).
static inline bool csi_motion(const float *s, int n, float threshold) {
  return csi_variance(s, n) > threshold;
}
