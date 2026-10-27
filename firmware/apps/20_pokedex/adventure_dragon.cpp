#include "adventure.hpp"
#include "dragon_trial_catalog.hpp"

namespace pokedex { namespace adventure {
namespace {
// A winning stage hands over at least one HP; the snapshot must stay a legal
// next-stage start value even if the battle ended on the last attack cue.
uint8_t carriedHp(const BattleView& view) {
  if (!view.playerHp) return 1;
  return view.playerHp <= dragongen::kPlayerHp ? view.playerHp : dragongen::kPlayerHp;
}
void resetEncounterForStage(Save& save) {
  save.encounter.usedBalls = 0;
  save.encounter.legacyFreeAttempts = 0;
  for (auto& attempt : save.encounter.attempts) attempt = ThrowAttempt();
  save.phase = Phase::EncounterReady;
  save.result = Result::None;
}
}

CommandResult Engine::beginDragonTrial(uint8_t trialId, uint32_t seed) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!dragonTrialExists(trialId)) return CommandResult::InvalidInput;
  // Qualification is read from the unified 251 capture counts every time; the
  // trial itself keeps no unlock flag. An already captured target may be replayed
  // only while this same 6-multiple window is open.
  if (!dragonTicketOpen(state_)) return CommandResult::NotReady;
  if (state_.phase != Phase::Walking) return CommandResult::InvalidPhase;
  if (!state_.starter || state_.clock.pending.active) return CommandResult::InvalidPhase;
  if (state_.nextEncounterId == UINT64_MAX || state_.activityEncounterCount == UINT64_MAX)
    return CommandResult::InvalidInput;
  const uint16_t species = dragonTrialSpecies(trialId);
  stage();
  candidate_.dragonTrial = DragonTrialState();
  candidate_.dragonTrial.active = true;
  candidate_.dragonTrial.trialId = trialId;
  candidate_.dragonTrial.stageIndex = 0;
  candidate_.dragonTrial.stageStartHp = dragongen::kPlayerHp;
  openActivity(candidate_, species, species, state_.route, seed, EncounterOrigin::DragonTrial);
  return submit();
}

CommandResult Engine::finishDragonBattle(const Encounter& current, Result outcome) {
  const DragonTrialState snapshot = state_.dragonTrial;
  stage();
  if (outcome != Result::Won) {
    // A defeat or retreat closes only this stage; the snapshot (stage start HP,
    // bonuses, guard) is already the retry source and stays untouched.
    candidate_.phase = Phase::Resolved;
    candidate_.result = outcome;
    return submit();
  }
  if (!dragonStageIsFinal(snapshot.trialId, snapshot.stageIndex)) {
    const uint8_t hp = carriedHp(battle_.view());
    candidate_.dragonTrial.stageIndex = uint8_t(snapshot.stageIndex + 1);
    candidate_.dragonTrial.stageStartHp = hp;
    if (dragonAnyUpgradeAvailable(candidate_.dragonTrial, hp)) {
      candidate_.phase = Phase::Resolved;
      candidate_.result = Result::DragonUpgrade;
    } else {
      // Defensive: the fixed catalog always offers an upgrade here, but a future
      // table without one must not deadlock the stage.
      candidate_.phase = Phase::EncounterReady;
      candidate_.result = Result::None;
    }
    return submit();
  }
  if (candidate_.captureCounts[current.species - 1]) {
    // Replaying a captured target owes no throw, card, ball or counter.
    candidate_.phase = Phase::Resolved;
    candidate_.result = Result::DragonCleared;
  } else {
    candidate_.phase = Phase::ThrowReady;
    candidate_.result = Result::Won;
  }
  return submit();
}

CommandResult Engine::chooseDragonUpgrade(uint8_t choice) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Resolved || state_.result != Result::DragonUpgrade)
    return CommandResult::InvalidPhase;
  const Encounter& e = state_.encounter;
  if (e.origin != EncounterOrigin::DragonTrial) return CommandResult::InvalidPhase;
  const DragonTrialState d = state_.dragonTrial;
  if (!d.active || d.stageIndex == 0 || d.stageIndex >= dragonTrialStageCount(d.trialId))
    return CommandResult::InvalidPhase;
  bool available = false;
  switch (choice) {
    case 0: available = dragonHealAvailable(d.stageStartHp); break;
    case 1: available = dragonWindowUpgradeAvailable(d); break;
    case 2: available = dragonGuardAvailable(d); break;
    default: return CommandResult::InvalidInput;
  }
  // The UI disables the same three rows; an unavailable choice never commits.
  if (!available) return CommandResult::InvalidInput;
  stage();
  if (choice == 0) candidate_.dragonTrial.stageStartHp = uint8_t(d.stageStartHp + 1);
  else if (choice == 1) {
    candidate_.dragonTrial.normalBonus =
        uint8_t(d.normalBonus + dragongen::kNormalBonusStepMs);
    candidate_.dragonTrial.perfectBonus =
        uint8_t(d.perfectBonus + dragongen::kPerfectBonusStepMs);
  } else candidate_.dragonTrial.guard = 1;
  // The next-stage snapshot is committed once here; repeating the input finds
  // Phase::EncounterReady and cannot advance or strengthen twice.
  resetEncounterForStage(candidate_);
  return submit();
}

CommandResult Engine::retryDragonTrial(uint64_t id, uint32_t seed) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Resolved) return CommandResult::InvalidPhase;
  if (!id || id != state_.encounter.id) return CommandResult::InvalidInput;
  const Encounter current = state_.encounter;
  if (current.origin != EncounterOrigin::DragonTrial) return CommandResult::InvalidPhase;
  const DragonTrialState d = state_.dragonTrial;
  if (!d.active) return CommandResult::InvalidPhase;
  if (state_.result == Result::Lost || state_.result == Result::Retreated) {
    // Same activity ID and seed: only the current stage is replayed.
    stage();
    resetEncounterForStage(candidate_);
    return submit();
  }
  if (state_.result != Result::Escaped && state_.result != Result::Released)
    return CommandResult::InvalidPhase;
  if (!dragonStageIsFinal(d.trialId, d.stageIndex)) return CommandResult::InvalidPhase;
  if (state_.nextEncounterId == UINT64_MAX || state_.activityEncounterCount == UINT64_MAX)
    return CommandResult::InvalidInput;
  // A capture retry closes the old result and opens a fresh final-stage attempt:
  // new ID and seed, clean throw record, unchanged stage snapshot and upgrades.
  stage();
  openActivity(candidate_, current.species, current.opponent, current.route, seed,
               EncounterOrigin::DragonTrial);
  return submit();
}
} }
