// Pure covert-transmitter band classifier for the audio-bug sweep — host-testable.
// A "bug sweep" hunts for a hidden mic/transmitter by finding RF energy where a
// covert device would sit. This file names the band a detected peak falls in.
// The CC1101/WiFi power sweep is bring-up; this is the frequency->band mapping.
#pragma once
#include <stddef.h>

// Name the covert-transmitter band a frequency (MHz) falls in, or NULL if none.
static inline const char *bug_band(float mhz) {
  if (mhz >= 88   && mhz <= 108)  return "FM covert mic";
  if (mhz >= 130  && mhz <= 175)  return "VHF bug";
  if (mhz >= 380  && mhz <= 480)  return "UHF bug";
  if (mhz >= 830  && mhz <= 915)  return "GSM/cell bug";
  if (mhz >= 1710 && mhz <= 1785) return "GSM1800 bug";
  if (mhz >= 2400 && mhz <= 2483) return "2.4GHz bug";
  return nullptr;
}

static inline bool is_bug_band(float mhz) { return bug_band(mhz) != nullptr; }
