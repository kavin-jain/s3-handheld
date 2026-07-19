// Pure A/C command-state helpers for the universal remote — host-testable.
// IRremoteESP8266's IRac synthesises a full A/C frame from (protocol, power, mode,
// temp, fan) at send time, so there are no per-brand code tables — just a valid
// state to hand it. This clamps/labels that state; the IRac send is bring-up.
#pragma once

enum AcMode { AC_COOL = 0, AC_HEAT, AC_DRY, AC_FAN, AC_AUTO, AC_MODE_N };
enum AcFan  { AC_FAN_AUTO = 0, AC_FAN_LOW, AC_FAN_MED, AC_FAN_HIGH, AC_FAN_N };

// Clamp to the range essentially every A/C accepts (Celsius).
static inline int ac_clamp_temp(int t) { return t < 16 ? 16 : t > 30 ? 30 : t; }

static inline const char *ac_mode_name(int m) {
  switch (m) {
    case AC_COOL: return "Cool";
    case AC_HEAT: return "Heat";
    case AC_DRY:  return "Dry";
    case AC_FAN:  return "Fan";
    case AC_AUTO: return "Auto";
    default:      return "?";
  }
}

static inline const char *ac_fan_name(int f) {
  switch (f) {
    case AC_FAN_AUTO: return "Auto";
    case AC_FAN_LOW:  return "Low";
    case AC_FAN_MED:  return "Med";
    case AC_FAN_HIGH: return "High";
    default:          return "?";
  }
}
