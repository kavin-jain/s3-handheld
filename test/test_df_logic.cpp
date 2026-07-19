// Host unit test for the pure direction-finder trend logic.
//   g++ -std=c++17 test/test_df_logic.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/df_logic.h"
#include <cassert>
#include <cstring>

int main() {
  assert(df_trend(-60, -70) == 1);    // stronger -> warmer
  assert(df_trend(-80, -70) == -1);   // weaker -> colder
  assert(df_trend(-71, -70) == 0);    // within deadband -> steady
  assert(strcmp(df_label(1), "WARMER") == 0);
  assert(strcmp(df_label(-1), "COLDER") == 0);
  assert(strcmp(df_label(0), "steady") == 0);
  return 0;
}
