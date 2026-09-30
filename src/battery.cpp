#include "battery.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX1704X.h>

static Adafruit_MAX17048 gauge;
static bool s_present = false;

bool batt_begin() {
  s_present = gauge.begin(&Wire);   // shares the Wire bus MCP23017/PN532 already started
  return s_present;
}

bool batt_present() { return s_present; }

int batt_pct() {
  if (!s_present) return 0;
  float p = gauge.cellPercent();
  if (p < 0)   p = 0;
  if (p > 100) p = 100;
  return (int)(p + 0.5f);
}
