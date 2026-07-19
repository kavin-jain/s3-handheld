// Pure I2C address decode + device ID for the Bus Pirate tool — host-testable.
// A bus scanner walks 0x08..0x77, pings each, and names what answers. The Wire
// transactions are bring-up; this file decodes an address byte and maps known
// 7-bit addresses to chip names (incl. this board's own I2C devices).
#pragma once
#include <stdint.h>

static inline uint8_t i2c_addr7(uint8_t byte)  { return byte >> 1; }   // drop R/W
static inline bool    i2c_is_read(uint8_t byte) { return byte & 1; }   // bit0 = read

static inline const char *i2c_device_name(uint8_t addr7) {
  switch (addr7) {
    case 0x20: return "MCP23017 GPIO";       // this board's button expander
    case 0x24: return "PN532 NFC";           // this board's NFC reader
    case 0x36: return "MAX17048 fuel gauge"; // this board's battery gauge
    case 0x3C: return "SSD1306 OLED";
    case 0x50: return "AT24 EEPROM";
    case 0x68: return "MPU6050 IMU";         // this board's accel/gyro
    case 0x69: return "MPU6050 (alt)";
    case 0x76: return "BMP/BME280";
    case 0x77: return "BMP/BME280 (alt)";
    default:   return "unknown";
  }
}
