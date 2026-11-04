#pragma once
#include "adventure_model.hpp"
#include "encounter_catalog.hpp"

namespace pokedex { namespace adventure { namespace save_validation {
inline bool cleanBitmap(const std::array<uint8_t, kSpeciesBytes>& bits, uint16_t count) {
  for (size_t byte = 0; byte < kSpeciesBytes; ++byte) {
    const uint16_t first = uint16_t(byte * 8 + 1);
    if (first > count) { if (bits[byte]) return false; continue; }
    const uint16_t used = uint16_t(count - first + 1);
    const uint8_t mask = used >= 8 ? 0xFF : uint8_t((1u << used) - 1);
    if (bits[byte] & uint8_t(~mask)) return false;
  }
  return true;
}
inline bool freshAttempts(const Encounter& e) {
  if (e.usedBalls || e.legacyFreeAttempts || e.ruleVersion != kRuleVersion) return false;
  for (const auto& a : e.attempts)
    if (a.used || a.timing != Timing::Miss || a.probability || a.roll || a.captured ||
        a.ruleVersion != kRuleVersion) return false;
  return true;
}
inline bool emptyEncounter(const Encounter& e) {
  return !e.id && !e.species && !e.partner && e.route == Route::Forest && !e.seed && freshAttempts(e);
}
inline bool validQueue(const Save& s) {
  if (s.encounterQueueSize > s.encounterQueue.size() ||
      (s.schema < 9 && s.encounterQueueSize) ||
      (s.phase == Phase::Walking && s.encounterQueueSize) ||
      s.encounterQueueSize >= s.nextEncounterId) return false;
  for (size_t i = 0; i < s.encounterQueue.size(); ++i) {
    const Encounter& e = s.encounterQueue[i];
    if (i >= s.encounterQueueSize) {
      if (!emptyEncounter(e)) return false;
      continue;
    }
    if (!s.starter || !freshAttempts(e) || !ordinaryEncounter(e.species) ||
        !isRegion(e.route) || regionOf(e.species) != e.route || !registered(s, e.partner) ||
        e.id != s.nextEncounterId - s.encounterQueueSize + i ||
        e.id <= s.legacyChargedCount + s.voidedLegacyCount ||
        s.captureCounts[e.species - 1] || e.species == s.encounter.species) return false;
    for (size_t j = 0; j < i; ++j)
      if (e.species == s.encounterQueue[j].species) return false;
  }
  return true;
}
} } }
