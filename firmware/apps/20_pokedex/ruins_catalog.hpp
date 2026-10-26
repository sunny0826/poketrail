#pragma once
#include "battle.hpp"

namespace pokedex { namespace adventure {

// The legendary ruins catalog is the only place that knows clue opponents, the
// required route order and the eleven boss parameters. Engine validation,
// battle setup and the pages all read it instead of branching on species.
constexpr uint8_t kRuinsClueTotal = 3;
constexpr uint8_t kRuinsAllClues = 0x07;
constexpr uint8_t kRuinsTargetTotal = 11;
constexpr uint8_t kRuinsNoIndex = 0xFF;

// Clue ids follow Route order: Forest = 0, Coast = 1, Cave = 2.
struct RuinsClueSpec {
  Route route = Route::Forest;
  uint16_t opponent = 0;
  uint8_t requiredPerfect = 0;   // 0 means a plain victory is enough
  bool requireFullHp = false;    // Cave has to be won without losing HP
  BattleSpec battle;             // default 3/3 HP, 12 cues, 1500ms, 180/70ms
};

struct RuinsTargetSpec {
  uint16_t species = 0;
  BattleSpec battle;
};

inline RuinsClueSpec ruinsClueAt(uint8_t index) {
  RuinsClueSpec spec;
  switch (index) {
    case 0: spec.route = Route::Forest; spec.opponent = 10; break;
    case 1: spec.route = Route::Coast; spec.opponent = 54; spec.requiredPerfect = 2; break;
    case 2: spec.route = Route::Cave; spec.opponent = 74; spec.requireFullHp = true; break;
    default: break;
  }
  return spec;
}

inline uint8_t ruinsClueIndexForRoute(Route route) {
  for (uint8_t index = 0; index < kRuinsClueTotal; ++index)
    if (ruinsClueAt(index).route == route) return index;
  return kRuinsNoIndex;
}

inline uint8_t ruinsClueIndexForOpponent(uint16_t opponent) {
  for (uint8_t index = 0; index < kRuinsClueTotal; ++index)
    if (ruinsClueAt(index).opponent == opponent) return index;
  return kRuinsNoIndex;
}

inline uint8_t ruinsClueBit(uint8_t index) { return uint8_t(1u << (index & 7)); }

// Route order puzzle: Coast -> Forest -> Cave.
inline Route ruinsOrderAt(uint8_t position) {
  switch (position) {
    case 0: return Route::Coast;
    case 1: return Route::Forest;
    case 2: return Route::Cave;
    default: return Route::Forest;
  }
}

inline uint8_t ruinsCorrectPosition(Route route) {
  for (uint8_t position = 0; position < kRuinsClueTotal; ++position)
    if (ruinsOrderAt(position) == route) return position;
  return kRuinsNoIndex;
}

// The 1-based slot a clue announces to the player: Coast 1, Forest 2, Cave 3.
// It follows the answer order, never the order the clues were collected in.
inline uint8_t ruinsClueOrderPosition(uint8_t clueIndex) {
  if (clueIndex >= kRuinsClueTotal) return 0;
  return uint8_t(ruinsCorrectPosition(ruinsClueAt(clueIndex).route) + 1);
}

inline RuinsTargetSpec ruinsTargetAt(uint8_t index) {
  RuinsTargetSpec spec;
  switch (index) {
    case 0: spec.species = 144; spec.battle.enemyHp = 4; spec.battle.beatMs = 1450;
      spec.battle.telegraph = TelegraphPattern::Alternate; break;
    case 1: spec.species = 145; spec.battle.enemyHp = 4; spec.battle.beatMs = 1450;
      spec.battle.telegraph = TelegraphPattern::AlternateDodgeFirst; break;
    case 2: spec.species = 146; spec.battle.enemyHp = 4; spec.battle.beatMs = 1400;
      spec.battle.telegraph = TelegraphPattern::AttackOnly; break;
    case 3: spec.species = 150; spec.battle.enemyHp = 5; spec.battle.beatMs = 1300;
      spec.battle.telegraph = TelegraphPattern::AttackOnly; break;
    case 4: spec.species = 151; spec.battle.enemyHp = 4; spec.battle.beatMs = 1400;
      spec.battle.telegraph = TelegraphPattern::AlternateDodgeFirst; break;
    case 5: spec.species = 243; spec.battle.enemyHp = 4; spec.battle.beatMs = 1450;
      spec.battle.telegraph = TelegraphPattern::AlternateDodgeFirst; break;
    case 6: spec.species = 244; spec.battle.enemyHp = 4; spec.battle.beatMs = 1400;
      spec.battle.telegraph = TelegraphPattern::AttackOnly; break;
    case 7: spec.species = 245; spec.battle.enemyHp = 4; spec.battle.beatMs = 1400;
      spec.battle.telegraph = TelegraphPattern::AlternateDodgeFirst; break;
    case 8: spec.species = 249; spec.battle.enemyHp = 5; spec.battle.beatMs = 1350;
      spec.battle.telegraph = TelegraphPattern::Alternate; break;
    case 9: spec.species = 250; spec.battle.enemyHp = 5; spec.battle.beatMs = 1350;
      spec.battle.telegraph = TelegraphPattern::AlternateDodgeFirst; break;
    case 10: spec.species = 251; spec.battle.enemyHp = 4; spec.battle.beatMs = 1400;
      spec.battle.telegraph = TelegraphPattern::Alternate; break;
    default: break;
  }
  // Every boss starts at full health; the player keeps the shared 3 HP, 12 cues,
  // 180/70ms windows and no missed-dodge guard.
  spec.battle.playerHp = 3;
  spec.battle.playerStartHp = 3;
  spec.battle.enemyStartHp = spec.battle.enemyHp;
  spec.battle.cueLimit = 12;
  spec.battle.normalWindowMs = kNormalWindowMs;
  spec.battle.perfectWindowMs = kPerfectWindowMs;
  spec.battle.missedDodgeGuard = 0;
  return spec;
}

inline uint8_t ruinsTargetIndex(uint16_t species) {
  for (uint8_t index = 0; index < kRuinsTargetTotal; ++index)
    if (ruinsTargetAt(index).species == species) return index;
  return kRuinsNoIndex;
}

inline bool ruinsTargetExists(uint16_t species) { return ruinsTargetIndex(species) != kRuinsNoIndex; }

inline uint8_t ruinsClueIndexForEncounter(const Encounter& e) {
  if (e.origin != EncounterOrigin::Ruins || e.species != e.opponent) return kRuinsNoIndex;
  return ruinsClueIndexForOpponent(e.opponent);
}

inline bool ruinsClueSatisfied(uint8_t index, const BattleView& view) {
  if (view.result != Result::Won) return false;
  const RuinsClueSpec spec = ruinsClueAt(index);
  if (spec.requiredPerfect && view.perfectCount < spec.requiredPerfect) return false;
  if (spec.requireFullHp && view.playerHp != view.playerMaxHp) return false;
  return true;
}

inline BattleSpec ruinsBattleSpec(const Encounter& e) {
  if (e.origin == EncounterOrigin::Ruins) {
    const uint8_t target = ruinsTargetIndex(e.species);
    if (target != kRuinsNoIndex) return ruinsTargetAt(target).battle;
    const uint8_t clue = ruinsClueIndexForOpponent(e.opponent);
    if (clue != kRuinsNoIndex) return ruinsClueAt(clue).battle;
  }
  return BattleSpec();
}

// Only a failed stage may be retried; a finished capture or clue stays closed.
inline bool ruinsActivityRetryable(const Encounter& e, Result result) {
  if (e.origin != EncounterOrigin::Ruins) return false;
  if (ruinsClueIndexForEncounter(e) != kRuinsNoIndex)
    return result == Result::Lost || result == Result::Retreated;
  if (!ruinsTargetExists(e.species)) return false;
  return result == Result::Lost || result == Result::Escaped ||
         result == Result::Retreated || result == Result::Released;
}

inline uint8_t ruinsCapturedTargets(const Save& s) {
  uint8_t count = 0;
  for (uint8_t index = 0; index < kRuinsTargetTotal; ++index)
    if (s.captureCounts[ruinsTargetAt(index).species - 1]) ++count;
  return count;
}

} }
