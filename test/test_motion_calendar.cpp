#include "support/motion_fixture.hpp"
#include "../firmware/apps/20_pokedex/calendar.hpp"
#include <limits>
#include <vector>

namespace {
using pokedex::calendar::CivilTime;
void calendarTests() {
  using namespace pokedex::calendar;
  int64_t utc = -1;
  check(toUtc({2000,1,1,0,0,0}, 0, utc) && utc == 946684800, "Unix epoch base");
  check(toUtc({2099,12,31,23,59,59}, 0, utc) && utc == 4102444799LL,
        "last second 2099");
  check(isLeapYear(2000) && !isLeapYear(2100) && isLeapYear(2024), "Gregorian century");
  check(isValid({2024,2,29,23,59,59}) && !isValid({2023,2,29,0,0,0}), "leap day");
  const CivilTime invalid[] = {{1999,12,31,0,0,0}, {2100,1,1,0,0,0},
    {2024,0,1,0,0,0}, {2024,13,1,0,0,0}, {2024,4,31,0,0,0},
    {2024,1,0,0,0,0}, {2024,1,1,24,0,0}, {2024,1,1,0,60,0},
    {2024,1,1,0,0,60}, {2024,1,1,-1,0,0}};
  for (const CivilTime& c : invalid) {
    utc = 42;
    uint32_t key = 42;
    check(!toUtc(c, 0, utc) && utc == 42, "invalid civil preserves output");
    check(!dayKey(c, key) && key == 42, "invalid key preserves output");
  }
  for (int offset : {-721, 841, std::numeric_limits<int>::max()}) {
    utc = 42;
    check(!toUtc({2026,1,1,0,0,0}, offset, utc) && utc == 42, "invalid offset");
  }
  CivilTime c = {2026,1,1,0,0,0};
  check(!toUtc({2000,1,1,0,0,0}, 840, utc), "lower UTC century boundary");
  check(!toUtc({2099,12,31,23,59,59}, -720, utc), "upper UTC century boundary");
  check(!fromUtc(946684800, -720, c) && c.year == 2026, "lower local boundary unchanged");
  check(!fromUtc(4102444799LL, 840, c), "upper local boundary");
  check(!fromUtc(std::numeric_limits<int64_t>::max(), 0, c), "huge UTC rejected");
  check(!fromUtc(-1, 0, c), "negative UTC rejected");
  check(toUtc({2024,3,1,0,0,0}, 480, utc), "local midnight conversion");
  check(fromUtc(utc, 0, c) && c.month == 2 && c.day == 29 && c.hour == 16,
        "positive offset crosses leap day");
  check(fromUtc(utc, 480, c) && c.month == 3 && c.day == 1 && c.hour == 0,
        "positive offset round trip");
  uint32_t key = 0;
  check(dayKey(c, key) && key == 20240301, "actual calendar day key");
  uint8_t week = 255;
  check(weekday({2000,1,1,0,0,0}, week) && week == 6, "Saturday weekday");
  // Every date in the supported century, including negative offsets and DST-free conversion.
  for (int64_t t = 946771200; t < 4102358400LL; t += 86400) {
    for (int offset : {-720, 0, 480, 840}) {
      int64_t recovered = 0;
      check(fromUtc(t, offset, c) && toUtc(c, offset, recovered) && recovered == t,
            "all supported dates round trip");
    }
  }
  // Confirm after midnight; assign each delayed peak with its original monotonic anchor.
  Feed midnight;
  midnight.cycle(); midnight.cycle(); midnight.cycle();
  check(toUtc({2026,10,22,23,59,59}, 480, utc), "midnight anchor");
  std::vector<uint32_t> keys;
  for (uint32_t at : midnight.steps) {
    check(fromUtc(utc + at / 1000, 480, c) && dayKey(c, key), "map actual event time");
    keys.push_back(key);
  }
  check(keys.size() == 3 && keys[0] == 20261022 && keys[1] == 20261022
        && keys[2] == 20261023, "delayed batch spans two days");
}
}
int main() {
  calendarTests();
  std::printf("PASS calendar: %u checks\n", checks);
}
