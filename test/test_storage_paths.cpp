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

  // sp_parse_seq: recover the sequence from a bare filename.
  assert(sp_parse_seq("nfc_0007.nfc", SAVE_NFC) == 7);
  assert(sp_parse_seq("sub_0042.sub", SAVE_SUBGHZ) == 42);
  assert(sp_parse_seq("duck_2345.txt", SAVE_BADUSB) == 2345);
  assert(sp_parse_seq("nfc_0007.nfc", SAVE_SUBGHZ) == -1);   // wrong kind prefix
  assert(sp_parse_seq("nfcX0007.nfc", SAVE_NFC) == -1);      // no underscore
  assert(sp_parse_seq("nfc_.nfc", SAVE_NFC) == -1);          // no digits
  assert(sp_parse_seq("nfc_0007", SAVE_NFC) == -1);          // no extension dot
  assert(sp_parse_seq(nullptr, SAVE_NFC) == -1);

  // Round-trip: make a path, strip the dir, parse the seq back.
  sp_make_path(b, sizeof b, SAVE_IR, "ir", 88);             // "/ir/ir_0088.ir"
  const char *base = b;
  for (const char *p = b; *p; p++) if (*p == '/') base = p + 1;
  assert(sp_parse_seq(base, SAVE_IR) == 88);
  return 0;  // all asserts held
}
