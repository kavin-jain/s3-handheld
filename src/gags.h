// Pure harmless USB-gag payload library — host-testable.
// Each entry is a DuckyScript line the "USB gag" tool would type. The test
// cross-checks every entry against ducky_parse() so the library can never drift
// out of sync with the parser. All harmless (open a page, lock screen, type).
// The USBHIDKeyboard playback is bring-up.
#pragma once
#include "ducky.h"

struct Gag { const char *name; const char *line; int expect; };

static const Gag GAGS[] = {
  {"Rickroll",  "STRING https://youtu.be/dQw4w9WgXcQ", DK_STRING},
  {"Open Run",  "GUI r",                                DK_GUI},
  {"Lock PC",   "GUI l",                                DK_GUI},
  {"Say hi",    "STRING hello there :)",                DK_STRING},
  {"Pause",     "DELAY 500",                            DK_DELAY},
  {"Press Enter","ENTER",                               DK_ENTER},
  {"Note",      "REM just a harmless gag",              DK_REM},
};
static const int GAG_COUNT = sizeof(GAGS) / sizeof(GAGS[0]);
