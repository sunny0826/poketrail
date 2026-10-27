#include "adventure_model.hpp"
#include "encounter_catalog.hpp"
#include "calendar.hpp"
#include <limits>
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
static bool validDate(int64_t utc, int16_t offset, uint32_t day) {
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
static bool validStamp(const Stamp& a, const Save& s) {
  if (a.totalSteps > s.totalSteps || a.dailySteps > a.totalSteps ||
      a.bootGeneration > s.bootGeneration || a.calibrationEpoch > s.clock.calibrationEpoch) return false;
  if (a.timeKnown) return validDate(a.utcSeconds, a.offsetMinutes, a.dayKey);
  return a.utcSeconds == 0 && a.offsetMinutes == 0 && a.dayKey == 0 &&
         a.dailySteps == 0 && !a.dayComplete;
}
static bool emptyStamp(const Stamp& a) {
  return !a.timeKnown && !a.dayComplete && !a.utcSeconds && !a.offsetMinutes && !a.dayKey &&
         !a.bootGeneration && !a.monotonicMs && !a.calibrationEpoch && !a.dailySteps && !a.totalSteps;
}
static bool validAchievement(const Achievement& a, const Save& s) {
  return validStamp(a.first, s) && (a.reached ? a.first.bootGeneration != 0 : emptyStamp(a.first));
}
static bool cleanBitmap(const std::array<uint8_t, kSpeciesBytes>& bits, uint16_t count) {
  for (size_t byte = 0; byte < kSpeciesBytes; ++byte) {
    const uint16_t first = uint16_t(byte * 8 + 1);
    if (first > count) { if (bits[byte]) return false; continue; }
    const uint16_t used = uint16_t(count - first + 1);
    const uint8_t mask = used >= 8 ? 0xFF : uint8_t((1u << used) - 1);
    if (bits[byte] & uint8_t(~mask)) return false;
  }
  return true;
}
static bool validCapture(const Capture& c, const Save& s) {
  return c.species <= kSpeciesCount && uint8_t(c.route) <= 2 && c.achievementMask <= 63 &&
         validStamp(c.at, s) && (c.species ? c.at.bootGeneration != 0 && c.at.totalSteps >= 500 :
         (emptyStamp(c.at) && c.route == Route::Forest && c.achievementMask == 0));
}
static bool validDay(const DayBucket& d, const Save& s) {
  if ((!d.dayKey && (d.steps || d.complete)) || (d.dayKey && !validDayKey(d.dayKey)) ||
      d.epoch > s.clock.calibrationEpoch || d.steps > s.totalSteps) return false;
  static const uint64_t thresholds[] = {1000,3000,5000};
  for (size_t i = 0; i < 3; ++i) {
    if (!validAchievement(d.milestones[i], s) ||
        d.milestones[i].reached != (d.steps >= thresholds[i])) return false;
    if (d.milestones[i].reached && (!d.milestones[i].first.timeKnown ||
        d.milestones[i].first.dayKey != d.dayKey || d.milestones[i].first.calibrationEpoch != d.epoch ||
        d.milestones[i].first.dailySteps < thresholds[i] || d.milestones[i].first.dailySteps > d.steps)) return false;
  }
  return true;
}
bool validate(const Save& s) {
  if (s.schema < 1 || s.schema > kSchemaVersion || uint8_t(s.route) > 2 || uint8_t(s.phase) > 4 ||
      uint8_t(s.result) > (s.schema == 1 ? 5 : 7) ||
      !cleanBitmap(s.owned, uint16_t(s.schema == 3 ? kSpeciesCount : 151)) || s.historySize > 14 ||
      s.recentSize > 32 || !s.nextEncounterId || s.explorationCredit > s.totalSteps ||
      s.undatedSteps > s.totalSteps || s.bestDaySteps > s.totalSteps) return false;
  const uint64_t encounters = s.nextEncounterId - 1;
  if (!s.bootGeneration && (s.totalSteps || s.lastStepBatch || s.starter)) return false;
  if (s.schema == 1) {
    if (encounters > s.totalSteps / 500 || s.explorationCredit != s.totalSteps - encounters * 500) return false;
  } else {
    if (s.voidedLegacyCount > 1 || s.legacyChargedCount > encounters ||
        s.voidedLegacyCount > encounters - s.legacyChargedCount ||
        s.legacyChargedCount > s.totalSteps / 500 || s.rewardBaseSteps > s.totalSteps ||
        s.ballsSpent > (s.totalSteps - s.rewardBaseSteps) / 1000 ||
        !cleanBitmap(s.registered, uint16_t(s.schema == 3 ? kSpeciesCount : 151))) return false;
    const uint64_t remaining = s.totalSteps - s.legacyChargedCount * 500;
    const uint64_t modern = encounters - s.legacyChargedCount - s.voidedLegacyCount;
    if (modern > remaining / 5000 || s.explorationCredit != remaining - modern * 5000) return false;
    uint64_t walked = 0;
    for (size_t i = 0; i < kSpeciesCount; ++i) {
      const SpeciesProgress& p = s.progress[i];
      if (p.spent > p.walked || p.walked > s.totalSteps - walked ||
          (!registered(s, uint16_t(i + 1)) && (p.walked || owns(s, uint16_t(i + 1)) || s.captureCounts[i]))) return false;
      walked += p.walked;
    }
    if (s.starter && (!registered(s, s.starter) || !s.rosterRevision)) return false;
    const EvolutionReceipt& receipt = s.lastEvolution;
    if (!validStamp(receipt.at, s)) return false;
    if (receipt.request.from) {
      const EvolutionRule* rule = evolutionRule(receipt.request.from, receipt.request.to);
      if (!rule || receipt.cost != rule->steps || !receipt.request.rosterRevision ||
          receipt.request.rosterRevision >= s.rosterRevision || !receipt.at.bootGeneration ||
          !registered(s, receipt.request.from) || !registered(s, receipt.request.to) ||
          s.progress[receipt.request.from - 1].spent < receipt.cost) return false;
    } else if (receipt.request.to || receipt.request.rosterRevision || receipt.cost || !emptyStamp(receipt.at)) return false;
  }
  if (s.starter != 0 && s.starter != 1 && s.starter != 4 && s.starter != 7) return false;
  if (s.starter ? ((s.schema == 1 && !owns(s, s.starter)) || !owns(s, s.partner)) : s.partner != 0) return false;
  uint64_t captures = 0;
  for (size_t i = 0; i < kSpeciesCount; ++i) {
    if (!validCapture(s.firstCapture[i], s) ||
        (s.captureCounts[i] ? s.firstCapture[i].species != i + 1 : s.firstCapture[i].species != 0) ||
        (s.schema == 1 && owns(s, uint16_t(i + 1)) != (s.captureCounts[i] != 0 || s.starter == i + 1))) return false;
    captures += s.captureCounts[i];
  }
  if (captures < s.recentSize || captures > encounters || (!s.starter && captures)) return false;
  for (size_t i = 0; i < 32; ++i) {
    if (!validCapture(s.recent[i], s) || ((i < s.recentSize) != (s.recent[i].species != 0))) return false;
    if (i < s.recentSize && !s.captureCounts[s.recent[i].species - 1]) return false;
    if (i < s.recentSize) {
      unsigned occurrences = 0;
      for (size_t j = 0; j < s.recentSize; ++j) occurrences += s.recent[j].species == s.recent[i].species;
      if (occurrences > s.captureCounts[s.recent[i].species - 1] ||
          s.firstCapture[s.recent[i].species - 1].at.totalSteps > s.recent[i].at.totalSteps ||
          (i && s.recent[i - 1].at.totalSteps > s.recent[i].at.totalSteps)) return false;
    }
  }
  if (!validDay(s.today, s) || s.today.steps > s.bestDaySteps) return false;
  uint64_t dated = s.today.steps;
  for (size_t i = 0; i < 14; ++i) {
    if (!validDay(s.history[i], s) || ((i < s.historySize) != (s.history[i].dayKey != 0))) return false;
    if (s.history[i].steps > s.bestDaySteps) return false;
    if (s.history[i].steps > s.totalSteps - dated) return false;
    dated += s.history[i].steps;
    if (i < s.historySize) {
      if (s.history[i].dayKey == s.today.dayKey && s.history[i].epoch == s.today.epoch) return false;
      for (size_t j = 0; j < i; ++j)
        if (s.history[i].dayKey == s.history[j].dayKey && s.history[i].epoch == s.history[j].epoch) return false;
    }
  }
  if (dated > s.totalSteps - s.undatedSteps) return false;
  static const uint64_t daily[] = {1000,3000,5000}, life[] = {10000,50000,100000};
  for (size_t i = 0; i < 3; ++i) {
    if (!validAchievement(s.dailyFirst[i], s) || !validAchievement(s.lifetime[i], s) ||
        s.lifetime[i].reached != (s.totalSteps >= life[i]) ||
        (s.lifetime[i].reached && s.lifetime[i].first.totalSteps < life[i]) ||
        (s.dailyFirst[i].reached && (!s.dailyFirst[i].first.timeKnown || s.dailyFirst[i].first.dailySteps < daily[i]))) return false;
  }
  const ClockState& c = s.clock;
  if (c.offsetMinutes < -720 || c.offsetMinutes > 840 ||
      (c.dayHighWater && !validDayKey(c.dayHighWater)) || !validReading(c.anchor) ||
      c.anchor.bootGeneration > s.bootGeneration || c.anchor.calibrationEpoch > c.calibrationEpoch ||
      (c.calibrated && !c.dayHighWater) ||
      (c.trusted && (!c.calibrated || c.pending.active || c.anomaly || !c.anchor.trusted || !s.today.dayKey)) ||
      (c.trusted && (c.anchor.offsetMinutes != c.offsetMinutes || c.anchor.calibrationEpoch != c.calibrationEpoch)) ||
      uint8_t(c.pending.mode) > 2 || (c.pending.active && !c.pending.transactionId) ||
      (c.pending.transactionId && !validDate(c.pending.targetUtcSeconds, c.pending.targetOffsetMinutes, c.pending.targetDayKey))) return false;
  if (s.today.dayKey && (s.today.epoch != c.calibrationEpoch || s.today.dayKey != c.dayHighWater)) return false;
  const Encounter& e = s.encounter;
  const uint64_t legacyBoundary = s.legacyChargedCount + s.voidedLegacyCount;
  const uint16_t rule = s.schema == 1 || (e.id && e.id <= legacyBoundary) ? 1 : 2;
  // Schema 1 stores neither ballsSpent nor legacyFreeAttempts; every later schema
  // owns spent balls, so the lower bound and the retired/legacy encounter
  // boundaries below must cover schema 2 and schema 3 alike.
  if (uint8_t(e.route) > 2 || e.usedBalls > 2 || e.ruleVersion != rule ||
      e.legacyFreeAttempts > e.usedBalls || (rule == 2 && e.legacyFreeAttempts) ||
      (s.schema != 1 && s.ballsSpent < uint64_t(e.usedBalls - e.legacyFreeAttempts))) return false;
  if (s.schema != 1 && e.id) {
    if (rule == 2 && !ordinaryEncounter(e.species)) return false;
    if (s.result == Result::MigrationRetired) {
      if (s.voidedLegacyCount != 1 || e.id != legacyBoundary || ordinaryEncounter(e.species)) return false;
    } else if (s.voidedLegacyCount && e.id == legacyBoundary) return false;
    if (rule == 1 && s.phase != Phase::Resolved && !ordinaryEncounter(e.species)) return false;
  }
  if (s.phase == Phase::Walking) {
    if (e.id || e.species || e.partner || e.usedBalls || s.result != Result::None) return false;
  } else {
    const CatalogEntry* entry = catalogEntry(e.species);
    if (!e.id || e.id != s.nextEncounterId - 1 || !entry || !(entry->routes & (1u << uint8_t(e.route))) ||
        !owns(s, e.partner)) return false;
    if ((s.phase == Phase::EncounterReady || s.phase == Phase::Battle) && (e.usedBalls || s.result != Result::None)) return false;
    if (s.phase == Phase::ThrowReady && (s.result != Result::Won || e.usedBalls > 1)) return false;
    if (s.phase == Phase::Resolved && s.result != Result::Lost && s.result != Result::Retreated &&
        s.result != Result::Captured && s.result != Result::Escaped &&
        s.result != Result::Released && s.result != Result::MigrationRetired) return false;
  }
  for (size_t i = 0; i < 2; ++i) {
    const ThrowAttempt& a = e.attempts[i];
    if (a.used != (i < e.usedBalls) || uint8_t(a.timing) > 3 || a.ruleVersion != rule || a.roll > 99) return false;
    if (a.used) {
      const CatalogEntry* entry = catalogEntry(e.species);
      if (!entry || a.probability != captureProbability(entry->rare, a.timing) ||
          a.roll != throwRoll(e, uint8_t(i + 1)) || a.captured != (a.roll < a.probability)) return false;
      if (a.captured && (i + 1 != e.usedBalls || s.result != Result::Captured)) return false;
    } else if (a.captured || a.probability || a.roll || a.timing != Timing::Miss) return false;
  }
  if (s.result == Result::Captured && (!e.usedBalls || !e.attempts[e.usedBalls - 1].captured || !s.captureCounts[e.species - 1])) return false;
  if ((s.result == Result::Released || s.result == Result::MigrationRetired) && e.usedBalls > 1) return false;
  if (s.result == Result::Escaped && e.usedBalls != 2) return false;
  if ((s.result == Result::Lost || s.result == Result::Retreated) && e.usedBalls) return false;
  return true;
}
} }
