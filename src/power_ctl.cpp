#include "power_ctl.h"
#include "power_level.h"

static int s_power = PWR_MAX;    // default: full send, per Kavin's "max the hardware"

int  power_level(void)        { return s_power; }
void set_power_level(int lvl)  { s_power = pwr_clamp(lvl); }
