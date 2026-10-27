#include "adventure_model.hpp"
#include "dragon_trial_catalog.hpp"
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
// A dragon trial saves the stage that is about to be fought: after a non-final
// win the snapshot already advanced to the next stage, so the upgrade page only
// changes HP or windows inside that stage.
static bool validDragonEncounter(const Save& s) {
  const Encounter& e = s.encounter;
  const DragonTrialState& d = s.dragonTrial;
  if (!d.active || !dragonTrialExists(d.trialId)) return false;
  if (e.species != dragonTrialSpecies(d.trialId) || e.opponent != e.species) return false;
  if (d.stageIndex >= dragonTrialStageCount(d.trialId)) return false;
  const bool finalStage = dragonStageIsFinal(d.trialId, d.stageIndex);
  switch (s.phase) {
    case Phase::EncounterReady:
    case Phase::Battle:
      if (e.usedBalls || s.result != Result::None) return false;
      break;
    case Phase::ThrowReady:
      if (!finalStage || s.result != Result::Won || e.usedBalls > 1) return false;
      // A captured target never reopens the throw; replay wins close with no reward.
      if (s.captureCounts[e.species - 1]) return false;
      break;
    case Phase::Resolved:
      switch (s.result) {
        case Result::Lost:
        case Result::Retreated:
          if (e.usedBalls) return false;
          break;
        case Result::Escaped:
          if (!finalStage || e.usedBalls != 2) return false;
          break;
        case Result::Captured:
          if (!finalStage || !e.usedBalls || !s.captureCounts[e.species - 1]) return false;
          break;
        case Result::Released:
          if (!finalStage || e.usedBalls > 1) return false;
          break;
        case Result::DragonUpgrade:
          // The snapshot already points at the next stage, which may be the
          // final one. Stage 0 can never be a pending upgrade, and a wait with
          // no selectable row would strand the page with no way forward.
          if (d.stageIndex == 0 || e.usedBalls) return false;
          if (!dragonAnyUpgradeAvailable(d, d.stageStartHp)) return false;
          break;
        case Result::DragonCleared:
          if (!finalStage || e.usedBalls || !s.captureCounts[e.species - 1]) return false;
          break;
        default:
          return false;
      }
      break;
    default:
      return false;
  }
  return true;
}
static bool validActivityEncounter(const Save& s) {
  const Encounter& e = s.encounter;
  if (e.origin == EncounterOrigin::Ordinary) return true;
  if (s.schema < 3) return false;
  if (!s.activityEncounterCount) return false;
  // Activity encounters share the one ID sequence but never use a legacy slot.
  if (e.id <= s.legacyChargedCount + s.voidedLegacyCount) return false;
  if (e.origin == EncounterOrigin::Ruins) return validRuinsEncounter(s);
  if (e.origin == EncounterOrigin::DragonTrial) return validDragonEncounter(s);
  return false;
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
    if (!dragonTrialExists(d.trialId)) return false;
    if (d.stageIndex >= dragonTrialStageCount(d.trialId)) return false;
    if (!d.stageStartHp || d.stageStartHp > dragongen::kPlayerHp) return false;
    if (!dragonBonusLegal(d.normalBonus, d.perfectBonus)) return false;
    if (d.guard > dragongen::kMaxGuard) return false;
    // Stage 0 is always the untouched trial opening; no choice can precede it.
    if (!d.stageIndex && !dragonInitialSnapshot(d)) return false;
    // One completed non-final stage buys at most one cumulative upgrade. A
    // pending wait has not spent the stage it points at yet. Heals leave no
    // cumulative trace, so the saved count may sit below this bound.
    const bool waiting = s.phase == Phase::Resolved && s.result == Result::DragonUpgrade;
    if (!dragonUpgradeBudgetLegal(d, waiting)) return false;
  } else if (d.trialId || d.stageIndex || d.stageStartHp || d.normalBonus ||
             d.perfectBonus || d.guard) {
    return false;
  }
  // The trial snapshot and the encounter slot describe the same fight.
  if (d.active != (s.encounter.origin == EncounterOrigin::DragonTrial)) return false;
  // A save that never issued an activity encounter keeps both activity states
  // empty. Ordinary encounters still store opponent == species.
  if (!s.activityEncounterCount && hasActivityState(s)) return false;
  if (s.encounter.origin != EncounterOrigin::Ordinary && !s.activityEncounterCount) return false;
  return validActivityEncounter(s);
}
} }
