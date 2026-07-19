// Host unit test for the pure device-settings serialize/parse.
//   g++ -std=c++17 test/test_config.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/config.h"
#include <cassert>
#include <cstring>

int main() {
  char buf[80];
  DeviceCfg a = {80, 20, 35, 0, 2};
  size_t n = cfg_serialize(&a, buf, sizeof buf);
  assert(n > 0);
  assert(strcmp(buf, "bright=80;dim=20;sleep=35;theme=0;power=2") == 0);

  // Round-trip into a defaulted struct.
  DeviceCfg b = {0, 0, 0, 0, 0};
  assert(cfg_parse(buf, &b));
  assert(b.bright == 80 && b.dim_s == 20 && b.sleep_s == 35 && b.theme == 0 && b.power == 2);

  // Partial input: only present keys change, others keep current values.
  DeviceCfg c = {100, 15, 30, 1, 2};
  cfg_parse("bright=50;power=0", &c);
  assert(c.bright == 50 && c.dim_s == 15 && c.sleep_s == 30 && c.theme == 1 && c.power == 0);

  // Unknown keys ignored; a key must match exactly (not a substring).
  DeviceCfg d = {100, 15, 30, 1, 2};
  cfg_parse("foo=9;dimmer=99;theme=2", &d);
  assert(d.theme == 2 && d.dim_s == 15);   // "dimmer" != "dim"

  // Overflow guard.
  assert(cfg_serialize(&a, buf, 4) == 0 && buf[0] == 0);
  assert(!cfg_parse(nullptr, &a));
  return 0;
}
