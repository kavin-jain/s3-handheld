// Host unit test for the pure skimmer-name heuristic.
//   g++ -std=c++17 test/test_skimmer.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/skimmer.h"
#include <cassert>

int main() {
  assert(is_skimmer_name("HC-05"));
  assert(is_skimmer_name("hc06"));               // case-insensitive
  assert(is_skimmer_name("JDY-31 module"));      // substring
  assert(is_skimmer_name("free2move-serial"));

  // Everyday devices are not flagged.
  assert(!is_skimmer_name("AirPods Pro"));
  assert(!is_skimmer_name("Galaxy Watch"));
  assert(!is_skimmer_name(""));
  assert(!is_skimmer_name(nullptr));
  return 0;
}
