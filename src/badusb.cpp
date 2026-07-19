#include "badusb.h"
#include "ducky.h"
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

// ponytail: HID is registered lazily (on first use), NOT at boot, so it doesn't
// disturb the CDC serial console during development. With ARDUINO_USB_CDC_ON_BOOT
// the USB stack is already up for CDC; adding HID here makes it composite.
// BRINGUP: verify the HID interface actually enumerates — some hosts need the HID
// class registered before the boot-time USB.begin(); if it doesn't show up, move
// badusb_begin() to setup() or drop CDC_ON_BOOT for pure-HID builds.
static USBHIDKeyboard Keyboard;
static bool s_ready = false;

bool badusb_begin() {
  if (s_ready) return true;
  Keyboard.begin();
  USB.begin();
  s_ready = true;
  return true;
}

bool badusb_ready() { return s_ready; }

void badusb_type(const char *text) {
  if (s_ready && text) Keyboard.print(text);
}

void badusb_run_line(const char *line) {
  if (!s_ready) return;
  char arg[128];
  switch (ducky_parse(line, arg, sizeof arg)) {
    case DK_STRING: Keyboard.print(arg); break;
    case DK_ENTER:  Keyboard.write(KEY_RETURN); break;
    case DK_DELAY:  delay(atoi(arg)); break;
    case DK_GUI:
      Keyboard.press(KEY_LEFT_GUI);
      if (arg[0]) Keyboard.press((uint8_t)arg[0]);
      delay(20);
      Keyboard.releaseAll();
      break;
    default: break;   // REM / NONE / KEYCOMBO (multi-key combos: later)
  }
}
