// Pure MBR / partition-table parse for the USB-host "read a flash drive" tool
// — host-testable. Plugging a USB stick in host mode, the first useful thing is
// its Master Boot Record: signature + up to four partition entries. The USB MSC
// enumeration + block reads are bring-up; this decodes sector 0.
#pragma once
#include <stdint.h>

#define MBR_PART0_OFF  446
#define MBR_PART_SIZE  16

// Valid MBR: the 0x55AA boot signature at bytes 510/511.
static inline bool mbr_valid(const uint8_t *sector) {
  return sector && sector[510] == 0x55 && sector[511] == 0xAA;
}

// Filesystem name from a partition type byte.
static inline const char *mbr_part_type(uint8_t t) {
  switch (t) {
    case 0x00: return "empty";
    case 0x01: return "FAT12";
    case 0x04:
    case 0x06: return "FAT16";
    case 0x07: return "NTFS/exFAT";
    case 0x0B:
    case 0x0C: return "FAT32";
    case 0x83: return "Linux";
    case 0xEE: return "GPT protective";
    default:   return "unknown";
  }
}

// Pointer to partition entry i (0..3), or NULL.
static inline const uint8_t *mbr_part(const uint8_t *sector, int i) {
  return (i >= 0 && i < 4) ? sector + MBR_PART0_OFF + i * MBR_PART_SIZE : 0;
}

// Little-endian u32 read at a byte offset within a 16-byte entry (inlined so
// this header stays independent of other LE helpers in the tree).
static inline uint32_t mbr_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint32_t mbr_part_lba(const uint8_t *entry)     { return mbr_le32(entry + 8); }
static inline uint32_t mbr_part_sectors(const uint8_t *entry) { return mbr_le32(entry + 12); }
