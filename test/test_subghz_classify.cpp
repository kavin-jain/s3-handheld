// Host unit test for the pure sub-GHz interpretation logic.
//   g++ -std=c++17 test/test_subghz_classify.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/subghz_classify.h"
#include <cassert>
#include <cstring>

int main() {
  int r[] = {-90, -60, -75};
  assert(sg_peak(r, 3) == 1);            // -60 dBm is strongest
  assert(sg_peak(nullptr, 0) == -1);

  assert(strcmp(sg_guess(433.92f), "Remote / gate / car fob / sensor") == 0);
  assert(strcmp(sg_guess(315.0f), "US remote / TPMS / key fob") == 0);
  assert(strcmp(sg_guess(868.0f), "EU ISM: sensor / LoRa / meter") == 0);
  assert(strcmp(sg_guess(915.0f), "US ISM: LoRa / sensor") == 0);
  assert(strcmp(sg_guess(50.0f), "Unknown sub-GHz band") == 0);

  assert(sg_bar_pct(-30) == 70);
  assert(sg_bar_pct(-200) == 0);         // clamps
  assert(sg_bar_pct(20) == 100);         // clamps
  return 0;
}
