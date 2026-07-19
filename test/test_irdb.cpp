// Host unit test for the pure IR brand code DB.
//   g++ -std=c++17 test/test_irdb.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/irdb.h"
#include <cassert>
#include <cstring>

int main() {
  assert(ir_brand_count() >= 2);

  const IrBrand *s = ir_find_brand("Samsung");
  assert(s && s->proto == IRP_SAMSUNG && s->power == 0xE0E040BF && s->bits == 32);

  const IrBrand *l = ir_find_brand("LG");
  assert(l && l->proto == IRP_NEC && l->power == 0x20DF10EF);

  // Index access + bounds.
  assert(ir_brand_at(0) == s || strcmp(ir_brand_at(0)->name, "Samsung") == 0);
  assert(ir_brand_at(-1) == nullptr);
  assert(ir_brand_at(ir_brand_count()) == nullptr);

  // Unknown / null.
  assert(ir_find_brand("Nokia") == nullptr);
  assert(ir_find_brand(nullptr) == nullptr);
  // Exact match only (no prefix match).
  assert(ir_find_brand("Sams") == nullptr);
  return 0;
}
