// Pure BLE tracker-identification heuristics — no Arduino, host-testable.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Apple Find My / AirTag: manufacturer data starts with the Apple company id
// (0x4C 0x00, little-endian) followed by the offline-finding type byte 0x12.
static inline bool ble_is_airtag(const uint8_t *mfg, size_t len) {
  return mfg && len >= 3 && mfg[0] == 0x4C && mfg[1] == 0x00 && mfg[2] == 0x12;
}

// Known trackers by 16-bit service UUID.
static inline const char *ble_tracker_by_uuid(uint16_t uuid) {
  switch (uuid) {
    case 0xFEED:
    case 0xFEEC: return "Tile";
    case 0xFD5A: return "Samsung SmartTag";
    default:     return nullptr;
  }
}

// Bluetooth SIG company id = first 2 bytes of manufacturer data (little-endian).
static inline uint16_t ble_company_id(const uint8_t *mfg, size_t len) {
  return (mfg && len >= 2) ? (uint16_t)(mfg[0] | (mfg[1] << 8)) : 0;
}

// Name a tracker brand from its company id (catches beacons that carry the
// company id but not the specific offline-finding type byte).
static inline const char *ble_tracker_brand(uint16_t company_id) {
  switch (company_id) {
    case 0x004C: return "Apple AirTag";
    case 0x0075: return "Samsung SmartTag";
    case 0x0157: return "Tile";
    default:     return nullptr;
  }
}
