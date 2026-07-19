// Host unit test for the pure storage path logic. Run on your machine, not the
// device:  g++ -std=c++17 test/test_storage_paths.cpp -o /tmp/t && /tmp/t && echo OK
// (PlatformIO's `pio run` ignores test/, so this never touches the firmware build.)
#include "../src/storage_paths.h"
#include <cassert>
#include <cstring>

int main() {
  assert(strcmp(sp_dir(SAVE_NFC), "/nfc") == 0);
  assert(strcmp(sp_dir(SAVE_SUBGHZ), "/subghz") == 0);

  char b[64];
  assert(sp_make_path(b, sizeof b, SAVE_NFC, "nfc", 7) > 0);
  assert(strcmp(b, "/nfc/nfc_0007.nfc") == 0);

  assert(sp_make_path(b, sizeof b, SAVE_SUBGHZ, "sub", 42) > 0);
  assert(strcmp(b, "/subghz/sub_0042.sub") == 0);

  assert(sp_make_path(b, sizeof b, SAVE_BADUSB, "txt", 12345) > 0);  // wraps % 10000
  assert(strcmp(b, "/badusb/duck_2345.txt") == 0);

  assert(sp_make_path(b, 4, SAVE_NFC, "nfc", 1) == 0 && b[0] == 0);  // overflow -> safe
  assert(sp_make_path(b, sizeof b, SAVE_KIND_N, "x", 1) == 0);       // bad kind
  assert(sp_make_path(nullptr, 10, SAVE_NFC, "n", 1) == 0);          // null out
  return 0;  // all asserts held
}
