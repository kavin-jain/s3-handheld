// Host unit test for the pure AC brand database.
//   g++ -std=c++17 test/test_ac_db.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ac_db.h"
#include <cassert>
#include <cstring>

int main() {
  assert(AC_BRAND_COUNT == 19);

  // A few brand -> protocol mappings (values from IRremoteESP8266 decode_type_t).
  assert(ac_find_brand("Daikin")->proto == 16);
  assert(ac_find_brand("Gree")->proto == 24);
  assert(ac_find_brand("LG")->proto == 51);
  assert(ac_find_brand("Voltas")->proto == 90);
  assert(strcmp(ac_brand_at(0)->name, "Coolix (generic)") == 0);

  // Bounds + unknown.
  assert(ac_brand_at(-1) == nullptr);
  assert(ac_brand_at(AC_BRAND_COUNT) == nullptr);
  assert(ac_find_brand("Nokia") == nullptr);
  assert(ac_find_brand(nullptr) == nullptr);

  // Every entry has a name and a plausible protocol id.
  for (int i = 0; i < AC_BRAND_COUNT; i++) {
    assert(ac_brand_at(i)->name && ac_brand_at(i)->name[0]);
    assert(ac_brand_at(i)->proto > 0);
  }
  return 0;
}
