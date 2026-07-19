// Host unit test for the pure iCal DTSTART parse.
//   g++ -std=c++17 test/test_ical.cpp -o /tmp/t && /tmp/t && echo OK
#include "../src/ical.h"
#include <cassert>
#include <cstring>

int main() {
  int y, mo, d, h, mi;
  assert(ical_parse_dt("20260719T143000Z", &y, &mo, &d, &h, &mi));
  assert(y == 2026 && mo == 7 && d == 19 && h == 14 && mi == 30);
  assert(strcmp(month_abbr(7), "Jul") == 0);

  // No trailing Z is fine.
  assert(ical_parse_dt("20260101T000000", &y, &mo, &d, &h, &mi));
  assert(mo == 1 && d == 1 && h == 0 && mi == 0);

  // Rejects: too short, bad month, bad hour, missing 'T'.
  assert(!ical_parse_dt("2026", &y, &mo, &d, &h, &mi));
  assert(!ical_parse_dt("20261319T000000", &y, &mo, &d, &h, &mi));
  assert(!ical_parse_dt("20260101T250000", &y, &mo, &d, &h, &mi));
  assert(!ical_parse_dt("20260101X120000", &y, &mo, &d, &h, &mi));

  char out[32];
  ical_friendly("20260719T143000Z", out, sizeof out);
  assert(strcmp(out, "Jul 19  14:30") == 0);
  ical_friendly("bogus", out, sizeof out);
  assert(out[0] == 0);

  // ical_next_event: first valid line wins; ts + title split; skips junk.
  char when[32], title[48];
  const char *feed =
    "not an event line\n"
    "20260720T091500Z Team standup\n"
    "20260101T000000 New year\n";
  assert(ical_next_event(feed, when, sizeof when, title, sizeof title));
  assert(strcmp(when, "Jul 20  09:15") == 0);
  assert(strcmp(title, "Team standup") == 0);
  // No valid line -> false, outputs cleared.
  assert(!ical_next_event("garbage\nmore\n", when, sizeof when, title, sizeof title));
  assert(when[0] == 0 && title[0] == 0);
  assert(!ical_next_event("", when, sizeof when, title, sizeof title));
  assert(!ical_next_event(nullptr, when, sizeof when, title, sizeof title));
  return 0;
}
