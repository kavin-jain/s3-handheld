// Pure device-settings serialize/parse — host-testable.
// Persists user prefs (brightness, sleep timers, theme) as a tiny key=value line
// so they survive a reboot. The SD read/write (storage.cpp) is bring-up; this is
// the encoding, round-trip tested. Unknown keys are ignored; missing keys keep
// whatever the struct already holds (so a partial/old file still loads).
#pragma once
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct DeviceCfg { int bright; int dim_s; int sleep_s; int theme; };

// "bright=80;dim=20;sleep=35;theme=0". Returns length, or 0 on overflow/bad args.
static inline size_t cfg_serialize(const DeviceCfg *c, char *out, size_t cap) {
  if (!c || !out || cap == 0) return 0;
  int n = snprintf(out, cap, "bright=%d;dim=%d;sleep=%d;theme=%d",
                   c->bright, c->dim_s, c->sleep_s, c->theme);
  if (n < 0 || (size_t)n >= cap) { out[0] = 0; return 0; }
  return (size_t)n;
}

// Read the int after "<key>=" (key at start or just after ';'); def if absent.
static inline int cfg_int_after(const char *s, const char *key, int def) {
  size_t kl = strlen(key);
  for (const char *p = s; *p; p++)
    if ((p == s || p[-1] == ';') && strncmp(p, key, kl) == 0 && p[kl] == '=')
      return atoi(p + kl + 1);
  return def;
}

// Overlay any keys present in s onto *c (missing keys keep their current value).
static inline bool cfg_parse(const char *s, DeviceCfg *c) {
  if (!s || !c) return false;
  c->bright  = cfg_int_after(s, "bright", c->bright);
  c->dim_s   = cfg_int_after(s, "dim",    c->dim_s);
  c->sleep_s = cfg_int_after(s, "sleep",  c->sleep_s);
  c->theme   = cfg_int_after(s, "theme",  c->theme);
  return true;
}
