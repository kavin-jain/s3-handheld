// Pure iCalendar DTSTART parse for the phone-bridged calendar — host-testable.
// The companion phone app pushes events over BLE as iCal lines; the device just
// needs to read the start time out of a "YYYYMMDDThhmmss[Z]" stamp and show it.
// The BLE bridge is bring-up; this is the timestamp parse + friendly format.
#pragma once
#include <stddef.h>
#include <stdio.h>

static inline int ical_num(const char *s, int off, int len) {
  int v = 0;
  for (int i = 0; i < len; i++) {
    char c = s[off + i];
    if (c < '0' || c > '9') return -1;
    v = v * 10 + (c - '0');
  }
  return v;
}

// Parse an iCal date-time "YYYYMMDDThhmmss" (optional trailing Z) into fields.
static inline bool ical_parse_dt(const char *s, int *y, int *mo, int *d,
                                 int *h, int *mi) {
  for (int i = 0; i < 15; i++) if (!s[i]) return false;
  if (s[8] != 'T') return false;
  *y = ical_num(s, 0, 4); *mo = ical_num(s, 4, 2); *d = ical_num(s, 6, 2);
  *h = ical_num(s, 9, 2); *mi = ical_num(s, 11, 2);
  return *y > 0 && *mo >= 1 && *mo <= 12 && *d >= 1 && *d <= 31 &&
         *h >= 0 && *h <= 23 && *mi >= 0 && *mi <= 59;
}

static inline const char *month_abbr(int mo) {
  static const char *m[] = {"?","Jan","Feb","Mar","Apr","May","Jun",
                            "Jul","Aug","Sep","Oct","Nov","Dec"};
  return (mo >= 1 && mo <= 12) ? m[mo] : "?";
}

// Friendly "Jul 19  14:30" into out; empty on parse failure.
static inline void ical_friendly(const char *dt, char *out, size_t cap) {
  int y, mo, d, h, mi;
  if (ical_parse_dt(dt, &y, &mo, &d, &h, &mi))
    snprintf(out, cap, "%s %d  %02d:%02d", month_abbr(mo), d, h, mi);
  else if (cap) out[0] = 0;
}
