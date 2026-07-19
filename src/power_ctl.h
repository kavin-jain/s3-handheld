// Runtime holder for the global intensity dial (see power_level.h).
// The radio wrappers call power_level() to pick their TX power; the UI/settings
// call set_power_level(). Kept in its own TU so every module shares one value.
#pragma once

int  power_level(void);          // current PWR_LOW / PWR_MED / PWR_MAX
void set_power_level(int lvl);   // clamped
