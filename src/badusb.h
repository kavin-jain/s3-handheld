// BadUSB / HID — act as a USB keyboard and run DuckyScript. Parser is in ducky.h.
#pragma once

bool badusb_begin();                    // register the HID keyboard (lazy; call before use)
bool badusb_ready();
void badusb_type(const char *text);     // type a literal string
void badusb_run_line(const char *line); // interpret one DuckyScript line
