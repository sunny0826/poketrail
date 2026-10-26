#include "adventure_model.hpp"
#include "ruins_catalog.hpp"
#include "adventure_model_internal.hpp"

namespace pokedex { namespace adventure {
bool noActivityHistory(const Save& s) {
  // Schema 1/2 never encode the activity block, so their in-memory Save must
  // stay at the exact legacy defaults.
  return !s.activityEncounterCount && s.encounter.origin == EncounterOrigin::Ordinary &&
         !s.encounter.opponent && !s.ruins.clueMask && !s.ruins.unlocked && !s.ruins.lastTarget &&
         !s.dragonTrial.active && !s.dragonTrial.trialId && !s.dragonTrial.stageIndex &&
         !s.dragonTrial.stageStartHp && !s.dragonTrial.normalBonus &&
         !s.dragonTrial.perfectBonus && !s.dragonTrial.guard;
}
static bool hasActivityState(const Save& s) {
  return s.ruins.clueMask || s.ruins.unlocked || s.ruins.lastTarget ||
         s.dragonTrial.active || s.dragonTrial.trialId || s.dragonTrial.stageIndex ||
         s.dragonTrial.stageStartHp || s.dragonTrial.normalBonus ||
         s.dragonTrial.perfectBonus || s.dragonTrial.guard;
}
static bool validRuinsEncounter(const Save& s) {
  const Encounter& e = s.encounter;
  if (e.origin == EncounterOrigin::Ordinary) return true;
  // DragonTrial stays reserved for its own catalog; no raise path exists yet.
  if (s.schema < 3 || e.origin != EncounterOrigin::Ruins) return false;
  if (!s.activityEncounterCount) return false;
  // Activity encounters share the one ID sequence but never use a legacy slot.
  if (e.id <= s.legacyChargedCount + s.voidedLegacyCount) return false;
  const uint8_t clue = ruinsClueIndexForEncounter(e);
  if (clue != kRuinsNoIndex) {
    const RuinsClueSpec spec = ruinsClueAt(clue);
    if (e.species != e.opponent || e.route != spec.route) return false;
    // Clue runs only exist before the puzzle unlocks the boss list.
    if (s.ruins.unlocked) return false;
    // A clue task never enters the throw stage and never awards a capture.
    if (e.usedBalls || e.attempts[0].used || e.attempts[1].used) return false;
    if (s.result == Result::Won || s.result == Result::Captured ||
        s.result == Result::Escaped || s.result == Result::Released ||
        s.result == Result::MigrationRetired || s.result == Result::RuinsCleared) return false;
    // The clue bit and the terminal result must agree. RuinsClue is the only
    // outcome that banks this route's clue; an unresolved, Lost or Retreated
    // run cannot already own it. Other routes' bits stay independent.
    if ((s.result == Result::RuinsClue) != bool(s.ruins.clueMask & ruinsClueBit(clue))) return false;
    return true;
  }
  const uint8_t target = ruinsTargetIndex(e.species);
  if (target == kRuinsNoIndex || e.species != e.opponent) return false;
  if (!s.ruins.unlocked || s.ruins.lastTarget != e.species) return false;
  if (s.result == Result::RuinsClue) return false;
  // Replaying a captured target keeps balls, counters and first-capture cards.
  if (s.result == Result::RuinsCleared &&
      (!s.captureCounts[e.species - 1] || e.usedBalls)) return false;
  return true;
}
bool validActivity(const Save& s) {
  if (s.ruins.clueMask >> kRuinsClueTotal) return false;
  if (s.ruins.lastTarget && !ruinsTargetExists(s.ruins.lastTarget)) return false;
  // Unlocking is monotonic and only follows all three clues; a chosen target
  // implies the list is already open.
  if (s.ruins.unlocked && s.ruins.clueMask != kRuinsAllClues) return false;
  if (s.ruins.lastTarget && !s.ruins.unlocked) return false;
  const DragonTrialState& d = s.dragonTrial;
  if (d.active) {
    if (!d.trialId || d.trialId > kDragonTrialCount) return false;
    if (d.stageIndex >= kDragonTrialStageCount) return false;
    if (!d.stageStartHp || d.stageStartHp > kMaxBattleHp) return false;
    if (d.normalBonus > kMaxWindowBonusMs || d.perfectBonus > kMaxWindowBonusMs) return false;
    if (d.guard > kMaxMissedDodgeGuard) return false;
  } else if (d.trialId || d.stageIndex || d.stageStartHp || d.normalBonus ||
             d.perfectBonus || d.guard) {
    return false;
  }
  // A save that never issued an activity encounter keeps both activity states
  // empty. Ordinary encounters still store opponent == species.
  if (!s.activityEncounterCount && hasActivityState(s)) return false;
  if (s.encounter.origin != EncounterOrigin::Ordinary && !s.activityEncounterCount) return false;
  return validRuinsEncounter(s);
}
} }
