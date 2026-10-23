#pragma once
#include <stdint.h>

namespace pokedex { namespace calendar {
struct CivilTime {
  int year, month, day, hour, minute, second;
};
bool isLeapYear(int year);
bool isValid(const CivilTime& value);
bool validOffset(int offsetMinutes);
bool toUtc(const CivilTime& local, int offsetMinutes, int64_t& utcSeconds);
bool fromUtc(int64_t utcSeconds, int offsetMinutes, CivilTime& local);
bool dayKey(const CivilTime& value, uint32_t& key);
bool weekday(const CivilTime& value, uint8_t& day);
} }
