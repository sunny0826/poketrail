#include "calendar.hpp"

namespace pokedex { namespace calendar {
namespace {
const int64_t kEpoch = 946684800LL;
const int64_t kEnd = 4102444800LL;
int monthDays(int year, int month) {
  static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return days[month - 1] + (month == 2 && isLeapYear(year) ? 1 : 0);
}
int daysSince2000(const CivilTime& value) {
  int days = 0;
  for (int y = 2000; y < value.year; ++y) days += isLeapYear(y) ? 366 : 365;
  for (int m = 1; m < value.month; ++m) days += monthDays(value.year, m);
  return days + value.day - 1;
}
}
bool isLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
bool validOffset(int offsetMinutes) {
  return offsetMinutes >= -720 && offsetMinutes <= 840;
}
bool isValid(const CivilTime& v) {
  return v.year >= 2000 && v.year <= 2099 && v.month >= 1 && v.month <= 12
      && v.day >= 1 && v.day <= monthDays(v.year, v.month)
      && v.hour >= 0 && v.hour < 24 && v.minute >= 0 && v.minute < 60
      && v.second >= 0 && v.second < 60;
}
bool toUtc(const CivilTime& local, int offsetMinutes, int64_t& utcSeconds) {
  if (!isValid(local) || !validOffset(offsetMinutes)) return false;
  const int64_t result = kEpoch + int64_t(daysSince2000(local)) * 86400
      + local.hour * 3600 + local.minute * 60 + local.second
      - int64_t(offsetMinutes) * 60;
  if (result < kEpoch || result >= kEnd) return false;
  utcSeconds = result;
  return true;
}
bool fromUtc(int64_t utcSeconds, int offsetMinutes, CivilTime& local) {
  if (!validOffset(offsetMinutes) || utcSeconds < kEpoch || utcSeconds >= kEnd)
    return false;
  const int64_t shifted = utcSeconds + int64_t(offsetMinutes) * 60;
  if (shifted < kEpoch || shifted >= kEnd) return false;
  int days = int((shifted - kEpoch) / 86400);
  const int seconds = int((shifted - kEpoch) % 86400);
  CivilTime result = {2000, 1, 1, seconds / 3600, seconds / 60 % 60, seconds % 60};
  while (days >= (isLeapYear(result.year) ? 366 : 365)) {
    days -= isLeapYear(result.year) ? 366 : 365;
    ++result.year;
  }
  while (days >= monthDays(result.year, result.month)) {
    days -= monthDays(result.year, result.month);
    ++result.month;
  }
  result.day = days + 1;
  local = result;
  return true;
}
bool dayKey(const CivilTime& value, uint32_t& key) {
  if (!isValid(value)) return false;
  key = uint32_t(value.year * 10000 + value.month * 100 + value.day);
  return true;
}
bool weekday(const CivilTime& value, uint8_t& day) {
  if (!isValid(value)) return false;
  day = uint8_t((daysSince2000(value) + 6) % 7);
  return true;
}
} }
