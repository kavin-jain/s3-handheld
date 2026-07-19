// Host unit test for the pure MBR / partition parse.
//   g++ -std=c++17 test/test_usbhost.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/usbhost.h"
#include <cassert>
#include <cstring>

int main() {
  uint8_t mbr[512] = {0};

  // No signature yet -> invalid.
  assert(!mbr_valid(mbr));
  mbr[510] = 0x55; mbr[511] = 0xAA;
  assert(mbr_valid(mbr));

  // Partition 0: type FAT32(LBA)=0x0C, LBA start 2048, 1,000,000 sectors.
  uint8_t *e = mbr + MBR_PART0_OFF;
  e[4] = 0x0C;
  e[8]  = 0x00; e[9]  = 0x08; e[10] = 0x00; e[11] = 0x00;   // 2048 LE
  e[12] = 0x40; e[13] = 0x42; e[14] = 0x0F; e[15] = 0x00;   // 1000000 LE

  assert(strcmp(mbr_part_type(e[4]), "FAT32") == 0);
  assert(mbr_part_lba(e) == 2048);
  assert(mbr_part_sectors(e) == 1000000);

  // Type table + bounds.
  assert(strcmp(mbr_part_type(0x07), "NTFS/exFAT") == 0);
  assert(strcmp(mbr_part_type(0x83), "Linux") == 0);
  assert(strcmp(mbr_part_type(0x99), "unknown") == 0);
  assert(mbr_part(mbr, 3) == mbr + MBR_PART0_OFF + 48);
  assert(mbr_part(mbr, 4) == nullptr);
  return 0;
}
