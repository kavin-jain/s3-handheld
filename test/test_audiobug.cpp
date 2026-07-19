// Host unit test for the pure covert-transmitter band classifier.
//   g++ -std=c++17 test/test_audiobug.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/audiobug.h"
#include <cassert>
#include <cstring>

int main() {
  assert(strcmp(bug_band(96.5f),  "FM covert mic") == 0);
  assert(strcmp(bug_band(160.0f), "VHF bug") == 0);
  assert(strcmp(bug_band(433.9f), "UHF bug") == 0);
  assert(strcmp(bug_band(900.0f), "GSM/cell bug") == 0);
  assert(strcmp(bug_band(2450.f), "2.4GHz bug") == 0);

  // Gaps between bands classify as clean.
  assert(bug_band(120.0f) == nullptr);
  assert(bug_band(600.0f) == nullptr);
  assert(!is_bug_band(600.0f));
  assert(is_bug_band(96.5f));
  return 0;
}
