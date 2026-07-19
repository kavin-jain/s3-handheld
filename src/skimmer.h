// Pure card-skimmer BLE-name heuristic — host-testable.
// Cheap credit-card skimmers relay data over generic Bluetooth modules that keep
// their factory names (HC-05/06, etc). Spotting one of those advertising at a
// pump or ATM is a strong tell. The BLE scan is bring-up; this is the match.
#pragma once
#include "strutil.h"

// True if a scanned device name matches a known default skimmer-module signature.
static inline bool is_skimmer_name(const char *name) {
  static const char *sigs[] = {
    "HC-05", "HC-06", "HC05", "HC06",     // classic serial BT modules
    "JDY-", "RNBT", "BT-BOARD", "free2move", "BLUESKIM",
  };
  for (const char *s : sigs)
    if (ci_contains(name, s)) return true;
  return false;
}
