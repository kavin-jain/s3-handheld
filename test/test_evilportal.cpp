// Host unit test for the pure Evil Portal form decode.
//   g++ -std=c++17 test/test_evilportal.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/evilportal.h"
#include <cassert>
#include <cstring>

int main() {
  char out[64];

  // Percent + '+' decoding.
  url_decode("p%40ss+word", out, sizeof out);
  assert(strcmp(out, "p@ss word") == 0);
  url_decode("%41%42%43", out, sizeof out);
  assert(strcmp(out, "ABC") == 0);

  const char *body = "user=admin&pass=p%40ss+word&remember=1";
  assert(form_get(body, "user", out, sizeof out));
  assert(strcmp(out, "admin") == 0);
  assert(form_get(body, "pass", out, sizeof out));
  assert(strcmp(out, "p@ss word") == 0);
  assert(form_get(body, "remember", out, sizeof out));
  assert(strcmp(out, "1") == 0);

  // Whole-key match only: "pass" must not match a "passcode" field.
  const char *body2 = "passcode=9999&x=1";
  assert(!form_get(body2, "pass", out, sizeof out));
  assert(form_get(body2, "passcode", out, sizeof out));
  assert(strcmp(out, "9999") == 0);

  // Absent key.
  assert(!form_get(body, "token", out, sizeof out));

  // Empty value.
  assert(form_get("user=&pass=x", "user", out, sizeof out));
  assert(out[0] == 0);
  return 0;
}
