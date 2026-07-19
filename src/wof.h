// Wall of Flipper — identify nearby hacker gear by its BLE advertised name.
// Pure/host-testable. ponytail: name-based; add manufacturer-data signatures
// (Flipper's company id, pwnagotchi service UUID) when ble_scan exposes them.
#pragma once
#include "strutil.h"

static inline const char *wof_identify(const char *name) {
  if (!name || !name[0]) return nullptr;
  if (ci_contains(name, "flipper"))  return "Flipper Zero";
  if (ci_contains(name, "pwnagotchi") || ci_contains(name, "pwn")) return "Pwnagotchi";
  if (ci_contains(name, "marauder")) return "WiFi Marauder";
  if (ci_contains(name, "bruce"))    return "Bruce device";
  return nullptr;
}
