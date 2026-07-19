// Pure OpenDroneID (Remote-ID) message decode — host-testable.
// Drones broadcast a Remote-ID beacon over BLE/WiFi; the "drone spotter" reads
// the operator/aircraft id and the live GPS position out of it. The radio
// capture is bring-up; this file decodes the message fields.
#pragma once
#include <stdint.h>

#define ODID_BASIC_ID 0
#define ODID_LOCATION 1

// The message-type is the high nibble of the header byte (low nibble = version).
static inline uint8_t odid_msg_type(uint8_t hdr) { return hdr >> 4; }

// Little-endian signed 32-bit read.
static inline int32_t le_i32(const uint8_t *p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

// Lat/Lon are stored as degrees * 1e7.
static inline double odid_coord(int32_t raw) { return (double)raw * 1e-7; }

// Copy the 20-byte ASCII UAS ID from a Basic ID message (starts at byte 2) into
// out[21], stopping at a NUL.
static inline void odid_basic_id(const uint8_t *msg, char out[21]) {
  int n = 0;
  for (int i = 0; i < 20; i++) {
    char c = (char)msg[2 + i];
    if (!c) break;
    out[n++] = c;
  }
  out[n] = 0;
}
