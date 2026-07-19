// Host unit test for the pure WiFi-CSI motion detector.
//   g++ -std=c++17 test/test_csi_motion.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/csi_motion.h"
#include <cassert>
#include <cmath>

int main() {
  // Amplitude of a 3-4-5 I/Q pair.
  assert(std::fabs(csi_amplitude(3, 4) - 5.0f) < 1e-4);

  // A still room: near-constant amplitude -> tiny variance -> no motion.
  float still[8] = {50.0f, 50.1f, 49.9f, 50.0f, 50.1f, 49.9f, 50.0f, 50.0f};
  assert(csi_variance(still, 8) < 0.5f);
  assert(!csi_motion(still, 8, 5.0f));

  // Someone walks through: amplitude swings hard -> big variance -> motion.
  float moving[8] = {40.0f, 60.0f, 42.0f, 58.0f, 39.0f, 61.0f, 41.0f, 59.0f};
  assert(csi_variance(moving, 8) > 50.0f);
  assert(csi_motion(moving, 8, 5.0f));

  // Mean sanity + empty guard.
  assert(std::fabs(csi_mean(still, 8) - 50.0f) < 0.1f);
  assert(csi_variance(nullptr, 0) == 0.f);
  return 0;
}
