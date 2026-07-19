// Pure GATT well-known-service-UUID -> name lookup — host-testable.
// The BLE connect + service enumeration itself is bring-up (BLEClient).
#pragma once
#include <stdint.h>

static inline const char *gatt_service_name(uint16_t u) {
  switch (u) {
    case 0x1800: return "Generic Access";
    case 0x1801: return "Generic Attribute";
    case 0x1802: return "Immediate Alert";
    case 0x1804: return "Tx Power";
    case 0x1809: return "Health Thermometer";
    case 0x180A: return "Device Info";
    case 0x180D: return "Heart Rate";
    case 0x180F: return "Battery";
    case 0x1812: return "HID";
    case 0x181A: return "Environmental";
    default:     return nullptr;
  }
}
