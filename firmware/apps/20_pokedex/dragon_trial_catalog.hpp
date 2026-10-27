#pragma once
#include "battle.hpp"
#include "dragon_trial_catalog_generated.hpp"

namespace pokedex { namespace adventure {

// Non-generated adapter over the fixed dragon trial catalog. The generated
// table is the only source of species, stage HP, beats and upgrade ceilings;
// Engine, validation and the pages all read these helpers instead of branching
// on species.
struct DragonStageSpec {
  uint16_t enemyHp = 0;
  uint32_t beatMs = 0;
};

inline bool dragonTrialExists(uint8_t trialId) {
  return trialId >= 1 && trialId <= dragongen::kTrialCount;
}
inline uint8_t dragonTrialStageCount(uint8_t trialId) {
  return dragonTrialExists(trialId) ? dragongen::kTrials[trialId - 1].stageCount : 0;
}
inline uint16_t dragonTrialSpecies(uint8_t trialId) {
  return dragonTrialExists(trialId) ? dragongen::kTrials[trialId - 1].species : 0;
}
inline DragonStageSpec dragonStageAt(uint8_t trialId, uint8_t stageIndex) {
  DragonStageSpec spec;
  if (!dragonTrialExists(trialId) || stageIndex >= dragonTrialStageCount(trialId)) return spec;
  const dragongen::StageEntry& entry =
      dragongen::kStages[dragongen::kTrials[trialId - 1].firstStage + stageIndex];
  spec.enemyHp = entry.enemyHp;
  spec.beatMs = entry.beatMs;
  return spec;
}
inline uint8_t dragonTrialForSpecies(uint16_t species) {
  for (uint8_t trialId = 1; trialId <= dragongen::kTrialCount; ++trialId)
    if (dragonTrialSpecies(trialId) == species) return trialId;
  return 0;
}
inline bool dragonStageIsFinal(uint8_t trialId, uint8_t stageIndex) {
  const uint8_t count = dragonTrialStageCount(trialId);
  return count && stageIndex + 1 == count;
}
inline uint32_t dragonNormalWindow(uint8_t bonusMs) {
  return uint32_t(dragongen::kNormalWindowMs) + bonusMs;
}
inline uint32_t dragonPerfectWindow(uint8_t bonusMs) {
  return uint32_t(dragongen::kPerfectWindowMs) + bonusMs;
}
// A saved bonus pair is legal only as whole 30/10 steps that advance together.
inline bool dragonBonusLegal(uint8_t normalBonus, uint8_t perfectBonus) {
  if (normalBonus > dragongen::kMaxNormalBonusMs || perfectBonus > dragongen::kMaxPerfectBonusMs)
    return false;
  if (normalBonus % dragongen::kNormalBonusStepMs || perfectBonus % dragongen::kPerfectBonusStepMs)
    return false;
  return normalBonus / dragongen::kNormalBonusStepMs ==
         perfectBonus / dragongen::kPerfectBonusStepMs;
}
inline bool dragonWindowUpgradeAvailable(const DragonTrialState& d) {
  return uint32_t(d.normalBonus) + dragongen::kNormalBonusStepMs <=
             dragongen::kMaxNormalBonusMs &&
         uint32_t(d.perfectBonus) + dragongen::kPerfectBonusStepMs <=
             dragongen::kMaxPerfectBonusMs;
}
inline bool dragonHealAvailable(uint8_t stageStartHp) {
  return stageStartHp >= 1 && stageStartHp < dragongen::kPlayerHp;
}
inline bool dragonGuardAvailable(const DragonTrialState& d) {
  return d.guard < dragongen::kMaxGuard;
}
// Each completed non-final stage spends at most one choice, and only window
// bonuses and the guard are cumulative. A heal changes the current stage HP but
// leaves no trace in this count, so the budget is an upper bound on saved
// upgrades rather than an exact tally of choices.
inline uint8_t dragonUpgradeCount(const DragonTrialState& d) {
  return uint8_t(d.normalBonus / dragongen::kNormalBonusStepMs + d.guard);
}
// A pending upgrade has not spent the current stage's choice yet, so it can
// carry one upgrade fewer than the stage it points at.
inline bool dragonUpgradeBudgetLegal(const DragonTrialState& d, bool waiting) {
  const uint8_t budget = waiting ? uint8_t(d.stageIndex - 1) : d.stageIndex;
  return dragonUpgradeCount(d) <= budget;
}
// The opening snapshot of stage 0 is always the untouched trial start.
inline bool dragonInitialSnapshot(const DragonTrialState& d) {
  return d.stageStartHp == dragongen::kPlayerHp && !d.normalBonus &&
         !d.perfectBonus && !d.guard;
}
inline bool dragonAnyUpgradeAvailable(const DragonTrialState& d, uint8_t stageStartHp) {
  return dragonHealAvailable(stageStartHp) || dragonWindowUpgradeAvailable(d) ||
         dragonGuardAvailable(d);
}
inline BattleSpec dragonBattleSpec(const DragonTrialState& d) {
  const DragonStageSpec stage = dragonStageAt(d.trialId, d.stageIndex);
  BattleSpec spec;
  spec.playerHp = dragongen::kPlayerHp;
  spec.playerStartHp = d.stageStartHp <= dragongen::kPlayerHp ? d.stageStartHp : dragongen::kPlayerHp;
  if (!spec.playerStartHp) spec.playerStartHp = 1;
  spec.enemyHp = stage.enemyHp;
  spec.enemyStartHp = stage.enemyHp;
  spec.cueLimit = dragongen::kCueLimit;
  spec.beatMs = stage.beatMs;
  spec.normalWindowMs = dragonNormalWindow(d.normalBonus);
  spec.perfectWindowMs = dragonPerfectWindow(d.perfectBonus);
  spec.telegraph = TelegraphPattern::Alternate;
  spec.missedDodgeGuard = d.guard;
  return spec;
}
// Only capture counts feed the ticket; starter gifts, owned/registered, evolutions
// and steps never do.
inline uint64_t dragonCaptureTotal(const Save& s) {
  uint64_t total = 0;
  for (size_t index = 0; index < kSpeciesCount; ++index) total += s.captureCounts[index];
  return total;
}
inline bool dragonTicketOpen(const Save& s) {
  const uint64_t total = dragonCaptureTotal(s);
  return total > 0 && total % dragongen::kTicketStep == 0;
}
// Remaining successes until the next opening; six at zero captures, zero while open.
inline uint8_t dragonCapturesUntilOpen(const Save& s) {
  const uint64_t total = dragonCaptureTotal(s);
  if (!total) return dragongen::kTicketStep;
  const uint64_t remainder = total % dragongen::kTicketStep;
  return remainder ? uint8_t(dragongen::kTicketStep - remainder) : 0;
}
inline bool dragonActivityRetryable(const Encounter& e, Result result) {
  if (e.origin != EncounterOrigin::DragonTrial) return false;
  return result == Result::Lost || result == Result::Retreated ||
         result == Result::Escaped || result == Result::Released;
}

} }
