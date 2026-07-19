// Host unit test for the pure I2C address decode + device ID.
//   g++ -std=c++17 test/test_buspirate.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/buspirate.h"
#include <cassert>
#include <cstring>

int main() {
  // Address byte 0x41 = 7-bit 0x20, read; 0x40 = 0x20, write.
  assert(i2c_addr7(0x41) == 0x20 && i2c_is_read(0x41));
  assert(i2c_addr7(0x40) == 0x20 && !i2c_is_read(0x40));
  assert(i2c_addr7(0x49) == 0x24 && i2c_is_read(0x49));   // PN532 read

  // Known chips (this board's bus).
  assert(strcmp(i2c_device_name(0x20), "MCP23017 GPIO") == 0);
  assert(strcmp(i2c_device_name(0x24), "PN532 NFC") == 0);
  assert(strcmp(i2c_device_name(0x36), "MAX17048 fuel gauge") == 0);
  assert(strcmp(i2c_device_name(0x68), "MPU6050 IMU") == 0);
  assert(strcmp(i2c_device_name(0x11), "unknown") == 0);
  return 0;
}
