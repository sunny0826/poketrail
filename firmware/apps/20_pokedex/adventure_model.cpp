#include "adventure_model.hpp"
#include "calendar.hpp"
#include "adventure_model_internal.hpp"

namespace pokedex { namespace adventure {
bool registered(const Save& s, uint16_t id) {
  return id >= 1 && id <= kSpeciesCount && (s.registered[(id - 1) / 8] & (1u << ((id - 1) % 8)));
}
void setRegistered(Save& s, uint16_t id) {
  if (id >= 1 && id <= kSpeciesCount) s.registered[(id - 1) / 8] |= uint8_t(1u << ((id - 1) % 8));
}
uint64_t availableBalls(const Save& s) {
  if (s.rewardBaseSteps > s.totalSteps) return 0;
  const uint64_t earned = (s.totalSteps - s.rewardBaseSteps) / 1000;
  return s.ballsSpent <= earned ? earned - s.ballsSpent : 0;
}
bool owns(const Save& s, uint16_t id) {
  return id >= 1 && id <= kSpeciesCount && (s.owned[(id - 1) / 8] & (1u << ((id - 1) % 8)));
}
void setOwned(Save& s, uint16_t id) {
  if (id >= 1 && id <= kSpeciesCount) s.owned[(id - 1) / 8] |= uint8_t(1u << ((id - 1) % 8));
}
bool validDayKey(uint32_t day) {
  const unsigned y = day / 10000, m = day / 100 % 100, d = day % 100;
  static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return y >= 2000 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 &&
         d <= unsigned(days[m - 1] + (m == 2 && y % 4 == 0));
}
bool validDate(int64_t utc, int16_t offset, uint32_t day) {
  calendar::CivilTime local;
  uint32_t derived = 0;
  return calendar::fromUtc(utc, offset, local) && calendar::dayKey(local, derived) && derived == day;
}
bool validReading(const ClockReading& r) {
  return !r.trusted || validDate(r.utcSeconds, r.offsetMinutes, r.dayKey);
}
bool consistentReading(const ClockReading& anchor, const ClockReading& r, bool allowLate) {
  if (!anchor.trusted || anchor.bootGeneration != r.bootGeneration) return true;
  const int32_t elapsed = int32_t(r.monotonicMs - anchor.monotonicMs);
  const int64_t drift = (r.utcSeconds - anchor.utcSeconds) * 1000 - elapsed;
  return (allowLate || elapsed >= 0) && drift >= -120000 && drift <= 120000;
}
uint32_t mixRandom(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16);
}
uint8_t throwRoll(const Encounter& e, uint8_t index) {
  uint32_t h = mixRandom(e.seed ^ uint32_t(e.id));
  h = mixRandom(h ^ uint32_t(e.id >> 32));
  h = mixRandom(h ^ (uint32_t(index) * 0x9e3779b9u) ^ (uint32_t(e.ruleVersion) << 16));
  return uint8_t(h % 100);
}
uint8_t captureProbability(bool rare, Timing timing) {
  static const uint8_t normal[] = {0,55,70,85}, special[] = {0,30,45,60};
  return uint8_t(timing) <= 3 ? (rare ? special : normal)[uint8_t(timing)] : 0;
}
uint16_t encounterOpponent(const Encounter& e) { return e.opponent ? e.opponent : e.species; }
Stamp makeStamp(const Save& s, const ClockReading& r) {
  Stamp a;
  a.bootGeneration = r.bootGeneration; a.monotonicMs = r.monotonicMs;
  a.calibrationEpoch = s.clock.calibrationEpoch; a.totalSteps = s.totalSteps;
  const DayBucket* bucket = nullptr;
  if (s.today.dayKey == r.dayKey && s.today.epoch == r.calibrationEpoch) bucket = &s.today;
  for (size_t i = 0; i < s.historySize; ++i)
    if (s.history[i].dayKey == r.dayKey && s.history[i].epoch == r.calibrationEpoch)
      bucket = &s.history[i];
  a.timeKnown = bucket && s.clock.trusted && !s.clock.pending.active && !s.clock.anomaly &&
    r.trusted && validReading(r) && r.bootGeneration == s.bootGeneration &&
    r.calibrationEpoch == s.clock.calibrationEpoch && r.offsetMinutes == s.clock.offsetMinutes &&
    consistentReading(s.clock.anchor, r, true);
  if (a.timeKnown) {
    a.utcSeconds = r.utcSeconds; a.offsetMinutes = r.offsetMinutes; a.dayKey = r.dayKey;
    a.dailySteps = bucket->steps; a.dayComplete = bucket->complete;
  }
  return a;
}
} }
