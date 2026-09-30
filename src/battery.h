// MAX17048 fuel gauge (I2C, addr 0x36) — battery %. Optional/deferred per the
// BOM; the status bar must never show a number it can't back with real data.
#pragma once

bool  batt_begin();     // init over the already-started I2C bus; true if the gauge answers
bool  batt_present();
int   batt_pct();       // 0-100, clamped. Only meaningful if batt_present().
