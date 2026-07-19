// Pure ESP32-S3 GPIO-safety classifier for the GPIO play tool — host-testable.
// On the S3-WROOM-1-N16R8, GPIO 26..32 are the SPI flash and 33..37 the octal
// PSRAM — driving any of them hangs or bricks the chip. 22..25 don't exist on
// the S3 at all. This gate stops the tool from ever toggling an unsafe pin.
#pragma once

// A pin number that physically exists on the ESP32-S3 (0..21, 26..48).
static inline bool gpio_valid(int pin) {
  return (pin >= 0 && pin <= 21) || (pin >= 26 && pin <= 48);
}

// Reserved by on-package flash (26..32) / PSRAM (33..37) — never drive these.
static inline bool gpio_reserved(int pin) {
  return pin >= 26 && pin <= 37;
}

// Safe for the user to toggle/read: exists and not reserved.
static inline bool gpio_usable(int pin) {
  return gpio_valid(pin) && !gpio_reserved(pin);
}
