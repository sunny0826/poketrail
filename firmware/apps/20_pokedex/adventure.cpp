#include "adventure.hpp"
#include "adventure_migration.hpp"
#include "dragon_trial_catalog.hpp"
#include "encounter_catalog.hpp"
#include "ruins_catalog.hpp"
#include <limits>
#include <new>
namespace pokedex { namespace adventure {
namespace {
// A captured clue opponent (10/54/74 are ordinary species) must not retire an
// active ruins clue run. A captured boss target only retires when its reward is
// still owed (ThrowReady); replay fights in EncounterReady survive a restart.
bool retiresWhenCaptured(const Encounter& e, Phase phase) {
  if (e.origin == EncounterOrigin::Ordinary) return true;
  return e.origin == EncounterOrigin::Ruins && ruinsTargetExists(e.species) &&
         phase == Phase::ThrowReady;
}
}
CommandResult Engine::guard() const {
  return pending_ ? CommandResult::SavePending : !ready_ ? CommandResult::NotReady : CommandResult::Applied;
}
void Engine::stage() { candidate_ = state_; }
CommandResult Engine::begin() {
  if (pending_) return CommandResult::SavePending;
  if (ready_) return CommandResult::Duplicate;
  loadResult_ = store_.load(candidate_);
  if (loadResult_.status != LoadStatus::Loaded && loadResult_.status != LoadStatus::Empty)
    return CommandResult::StorageError;
  if (loadResult_.status == LoadStatus::Empty) {
    candidate_.~Save();
    new (&candidate_) Save();
  }
  if (!validate(candidate_) || candidate_.bootGeneration == UINT32_MAX) {
    loadResult_.status = LoadStatus::Corrupt;
    return CommandResult::StorageError;
  }
  if (!migrateLegacy(candidate_)) { loadResult_.status = LoadStatus::Corrupt; return CommandResult::StorageError; }
  if (candidate_.phase != Phase::Walking && candidate_.phase != Phase::Resolved &&
      retiresWhenCaptured(candidate_.encounter, candidate_.phase) &&
      candidate_.captureCounts[candidate_.encounter.species - 1]) {
    candidate_.phase = Phase::Resolved;
    candidate_.result = Result::Released;
  }
  ++candidate_.bootGeneration;
  candidate_.lastStepBatch = 0;
  candidate_.clock.trusted = false;
  if (candidate_.phase == Phase::Battle) candidate_.phase = Phase::EncounterReady;
  bootPending_ = true;
  return submit();
}
CommandResult Engine::submit() {
  if (!pending_) {
    if (candidate_.sequence == UINT64_MAX) return CommandResult::InvalidInput;
    ++candidate_.sequence;
    pending_ = true;
  }
  if (!validate(candidate_)) return CommandResult::InvalidInput;
  if (store_.commit(candidate_) != CommitStatus::Committed) return CommandResult::StorageError;
  state_ = candidate_;
  pending_ = false;
  dirty_ = false;
  if (bootPending_) { ready_ = true; bootPending_ = false; }
  if (queueSize_ || overflow_) return flushSteps();
  return CommandResult::Applied;
}
CommandResult Engine::retrySave() {
  return pending_ ? submit() : CommandResult::Duplicate;
}
CommandResult Engine::checkpoint() {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if ((battle_.view().active && !battle_.view().paused) || throw_.active) return CommandResult::Deferred;
  if (queueSize_ || overflow_) {
    const CommandResult r = flushSteps();
    if (r != CommandResult::Applied && r != CommandResult::Duplicate) return r;
  }
  if (!dirty_) return CommandResult::Duplicate;
  stage(); return submit();
}
void Engine::createEncounter(Save& s, uint32_t seed) {
  if (s.phase != Phase::Walking || !s.starter || s.explorationCredit < kEncounterSteps ||
      s.nextEncounterId == UINT64_MAX) return;
  const uint16_t species = chooseEncounter(s, s.route, seed);
  if (!species) return;
  s.encounter = Encounter();
  s.encounter.id = s.nextEncounterId++;
  s.encounter.species = species;
  s.encounter.opponent = species;  // ordinary encounters fight the capture target
  s.encounter.partner = s.partner;
  s.encounter.route = s.route;
  s.encounter.seed = seed;
  s.explorationCredit -= kEncounterSteps;
  s.phase = Phase::EncounterReady;
  s.result = Result::None;
}
CommandResult Engine::chooseStarter(uint16_t id) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.starter) return state_.starter == id ? CommandResult::Duplicate : CommandResult::InvalidPhase;
  if (id != 1 && id != 4 && id != 7) return CommandResult::InvalidInput;
  if (state_.rosterRevision == UINT64_MAX) return CommandResult::InvalidInput;
  stage(); ++candidate_.rosterRevision; setRegistered(candidate_, id); candidate_.starter = candidate_.partner = id; setOwned(candidate_, id);
  createEncounter(candidate_, mixRandom(state_.bootGeneration));
  return submit();
}
CommandResult Engine::selectPartner(uint16_t id) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if ((state_.phase != Phase::Walking && state_.phase != Phase::EncounterReady) ||
      state_.clock.pending.active || (battle_.view().active && battle_.view().result == Result::None) || throw_.active) return CommandResult::InvalidPhase;
  if (queueSize_ || overflow_) {
    const CommandResult r = flushSteps();
    if (r != CommandResult::Applied && r != CommandResult::Duplicate) return r;
  }
  if (!owns(state_, id)) return CommandResult::InvalidInput;
  if (state_.partner == id) return CommandResult::Duplicate;
  if (state_.rosterRevision == UINT64_MAX) return CommandResult::InvalidInput;
  stage(); ++candidate_.rosterRevision; candidate_.partner = id;
  if (candidate_.phase == Phase::EncounterReady) candidate_.encounter.partner = id;
  return submit();
}
CommandResult Engine::selectRoute(Route route) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (uint8_t(route) > 2) return CommandResult::InvalidInput;
  if ((battle_.view().active && !battle_.view().paused) || throw_.active) return CommandResult::Deferred;
  if (state_.route == route) return CommandResult::Duplicate;
  stage(); candidate_.route = route;
  createEncounter(candidate_, mixRandom(state_.bootGeneration ^ uint32_t(state_.nextEncounterId)));
  return submit();
}
CommandResult Engine::startBattle(uint32_t now) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::EncounterReady || !owns(state_, state_.encounter.partner)) return CommandResult::InvalidPhase;
  // A restart keeps the activity encounter on disk and re-enters this path, so
  // the spec has to come from the catalog, never from the default boss numbers.
  if (state_.encounter.origin == EncounterOrigin::Ordinary) battle_.begin(now, state_.encounter.seed);
  else if (state_.encounter.origin == EncounterOrigin::Ruins)
    battle_.begin(now, state_.encounter.seed, ruinsBattleSpec(state_.encounter));
  else if (state_.encounter.origin == EncounterOrigin::DragonTrial) {
    // The saved stage snapshot, not the default or stage 1, supplies start HP,
    // actual windows and the guard after any restart.
    if (!state_.dragonTrial.active ||
        state_.dragonTrial.trialId != dragonTrialForSpecies(state_.encounter.species) ||
        state_.dragonTrial.stageIndex >= dragonTrialStageCount(state_.dragonTrial.trialId))
      return CommandResult::InvalidPhase;
    battle_.begin(now, state_.encounter.seed, dragonBattleSpec(state_.dragonTrial));
  } else return CommandResult::InvalidPhase;
  state_.phase = Phase::Battle; dirty_ = true;
  return CommandResult::Applied;
}
CommandResult Engine::finishBattle() {
  const Result outcome = battle_.view().result;
  if (outcome == Result::None) return CommandResult::Applied;
  const Encounter current = state_.encounter;
  if (current.origin == EncounterOrigin::DragonTrial)
    return finishDragonBattle(current, outcome);
  stage();
  if (current.origin == EncounterOrigin::Ruins) {
    const uint8_t clue = ruinsClueIndexForEncounter(current);
    if (clue != kRuinsNoIndex) {
      // A clue only exists after its own task succeeds; a won battle that misses
      // the requirement is a failed task, not a capture.
      if (outcome == Result::Won && ruinsClueSatisfied(clue, battle_.view())) {
        candidate_.ruins.clueMask |= ruinsClueBit(clue);
        candidate_.result = Result::RuinsClue;
      } else {
        candidate_.result = Result::Lost;
      }
      candidate_.phase = Phase::Resolved;
    } else if (outcome == Result::Won) {
      if (candidate_.captureCounts[current.species - 1]) {
        // Replaying a captured target skips the throw and every reward.
        candidate_.phase = Phase::Resolved;
        candidate_.result = Result::RuinsCleared;
      } else {
        candidate_.phase = Phase::ThrowReady;
        candidate_.result = Result::Won;
      }
    } else {
      candidate_.phase = Phase::Resolved;
      candidate_.result = Result::Lost;
    }
  } else {
    candidate_.result = outcome;
    candidate_.phase = outcome == Result::Won ? Phase::ThrowReady : Phase::Resolved;
  }
  return submit();
}
CommandResult Engine::press(Action action, uint32_t now) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Battle) return CommandResult::InvalidPhase;
  if (uint8_t(action) > 2) return CommandResult::InvalidInput;
  const bool accepted = battle_.press(action, now);
  if (battle_.view().result != Result::None) return finishBattle();
  if (battle_.view().paused && (queueSize_ || overflow_)) return flushSteps();
  return accepted ? CommandResult::Applied : CommandResult::Duplicate;
}
CommandResult Engine::tick(uint32_t now, const ClockReading& clock) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase == Phase::Battle) {
    battle_.tick(now);
    if (battle_.view().result != Result::None) return finishBattle();
    if (battle_.view().paused && (queueSize_ || overflow_)) return flushSteps();
    return CommandResult::Applied;
  }
  if (throw_.active && timeAfter(now, throw_.targetMs + 180))
    return resolveThrow(throw_.key, Timing::Miss, clock);
  return CommandResult::Duplicate;
}
CommandResult Engine::requestPause() {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  return state_.phase == Phase::Battle && battle_.requestPause() ? CommandResult::Applied : CommandResult::InvalidPhase;
}
CommandResult Engine::resume(uint32_t now) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  return state_.phase == Phase::Battle && battle_.resume(now) ? CommandResult::Applied : CommandResult::InvalidPhase;
}
CommandResult Engine::retreat() {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Battle && state_.phase != Phase::EncounterReady) return CommandResult::InvalidPhase;
  battle_ = Battle();
  stage(); candidate_.phase = Phase::Resolved; candidate_.result = Result::Retreated;
  return submit();
}
CommandResult Engine::acknowledgeResult(uint64_t id, uint32_t seed) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Resolved) return CommandResult::InvalidPhase;
  if (id != state_.encounter.id) return CommandResult::InvalidInput;
  // Activity results close through acknowledgeActivityResult, which never calls
  // createEncounter or reuses the ordinary credit.
  if (state_.encounter.origin != EncounterOrigin::Ordinary) return CommandResult::InvalidPhase;
  stage(); candidate_.phase = Phase::Walking; candidate_.result = Result::None;
  candidate_.encounter = Encounter(); createEncounter(candidate_, seed);
  return submit();
}
void Engine::openActivity(Save& s, uint16_t species, uint16_t opponent, Route route, uint32_t seed,
                          EncounterOrigin origin) {
  Encounter e;
  e.id = s.nextEncounterId++;
  e.species = species;
  e.opponent = opponent;
  e.partner = s.partner;
  e.route = route;
  e.seed = seed;
  e.origin = origin;
  s.encounter = e;
  ++s.activityEncounterCount;
  s.phase = Phase::EncounterReady;
  s.result = Result::None;
}
CommandResult Engine::startActivity(uint16_t species, uint16_t opponent, Route route, uint32_t seed) {
  if (!state_.starter || state_.clock.pending.active) return CommandResult::InvalidPhase;
  // Starting an activity takes the single encounter slot; it never charges the
  // 5000-step ordinary credit.
  if (state_.phase != Phase::Walking) return CommandResult::InvalidPhase;
  if (state_.nextEncounterId == UINT64_MAX || state_.activityEncounterCount == UINT64_MAX)
    return CommandResult::InvalidInput;
  stage();
  openActivity(candidate_, species, opponent, route, seed, EncounterOrigin::Ruins);
  return submit();
}
CommandResult Engine::beginRuinsClue(uint8_t clueIndex, uint32_t seed) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (clueIndex >= kRuinsClueTotal) return CommandResult::InvalidInput;
  if (state_.ruins.unlocked) return CommandResult::InvalidPhase;
  if (state_.ruins.clueMask & ruinsClueBit(clueIndex)) return CommandResult::Duplicate;
  const RuinsClueSpec clue = ruinsClueAt(clueIndex);
  return startActivity(clue.opponent, clue.opponent, clue.route, seed);
}
RuinsOrderResult Engine::solveRuinsOrder(const uint8_t* order) {
  RuinsOrderResult out;
  const CommandResult g = guard();
  if (g != CommandResult::Applied) { out.status = g; return out; }
  if (!order) { out.status = CommandResult::InvalidInput; return out; }
  for (uint8_t position = 0; position < kRuinsClueTotal; ++position) {
    if (order[position] > 2) {
      out.status = CommandResult::InvalidInput; out.conflict = position; return out;
    }
  }
  if (state_.ruins.unlocked) { out.status = CommandResult::Duplicate; return out; }
  if (state_.ruins.clueMask != kRuinsAllClues || state_.phase != Phase::Walking ||
      state_.clock.pending.active) {
    out.status = CommandResult::InvalidPhase; return out;
  }
  for (uint8_t position = 0; position < kRuinsClueTotal; ++position) {
    if (order[position] != uint8_t(ruinsOrderAt(position))) {
      // A wrong guess is a pure read: no ball, step or unlock changes.
      out.status = CommandResult::InvalidInput; out.conflict = position; return out;
    }
  }
  stage(); candidate_.ruins.unlocked = true;
  out.status = submit();
  return out;
}
CommandResult Engine::selectRuinsTarget(uint16_t species) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!state_.ruins.unlocked) return CommandResult::InvalidPhase;
  if (!ruinsTargetExists(species)) return CommandResult::InvalidInput;
  if (state_.encounter.origin != EncounterOrigin::Ordinary || state_.clock.pending.active)
    return CommandResult::InvalidPhase;
  if (state_.phase == Phase::Battle || state_.phase == Phase::ThrowReady ||
      state_.phase == Phase::Resolved) return CommandResult::InvalidPhase;
  if (state_.ruins.lastTarget == species) return CommandResult::Duplicate;
  stage(); candidate_.ruins.lastTarget = species;
  return submit();
}
CommandResult Engine::beginRuinsBoss(uint32_t seed) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  const uint8_t target = ruinsTargetIndex(state_.ruins.lastTarget);
  if (!state_.ruins.unlocked || target == kRuinsNoIndex) return CommandResult::InvalidPhase;
  return startActivity(state_.ruins.lastTarget, state_.ruins.lastTarget, state_.route, seed);
}
CommandResult Engine::retryRuinsActivity(uint64_t id, uint32_t seed) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Resolved) return CommandResult::InvalidPhase;
  if (!id || id != state_.encounter.id) return CommandResult::InvalidInput;
  const Encounter current = state_.encounter;
  if (current.origin != EncounterOrigin::Ruins) return CommandResult::InvalidPhase;
  const uint8_t clue = ruinsClueIndexForEncounter(current);
  if (clue != kRuinsNoIndex) {
    if (state_.ruins.clueMask & ruinsClueBit(clue)) return CommandResult::Duplicate;
    if (state_.result != Result::Lost && state_.result != Result::Retreated)
      return CommandResult::InvalidPhase;
  } else if (!ruinsTargetExists(current.species)) {
    return CommandResult::InvalidInput;
  } else if (state_.result != Result::Lost && state_.result != Result::Escaped &&
             state_.result != Result::Retreated && state_.result != Result::Released) {
    return CommandResult::InvalidPhase;
  }
  if (state_.nextEncounterId == UINT64_MAX || state_.activityEncounterCount == UINT64_MAX)
    return CommandResult::InvalidInput;
  // Retrying closes the finished encounter first; the new attempt gets its own
  // ID and seed and never overwrites an unclosed fight.
  stage();
  openActivity(candidate_, current.species, current.opponent, current.route, seed,
               EncounterOrigin::Ruins);
  return submit();
}
CommandResult Engine::acknowledgeActivityResult(uint64_t id) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Resolved) return CommandResult::InvalidPhase;
  if (!id || id != state_.encounter.id) return CommandResult::InvalidInput;
  if (state_.encounter.origin == EncounterOrigin::Ordinary) return CommandResult::InvalidPhase;
  // A pending upgrade is not a result: it must be chosen, never acknowledged away.
  if (state_.result == Result::DragonUpgrade) return CommandResult::InvalidPhase;
  stage();
  // Returning from a dragon result abandons that run; a retry keeps the snapshot.
  if (state_.encounter.origin == EncounterOrigin::DragonTrial)
    candidate_.dragonTrial = DragonTrialState();
  candidate_.phase = Phase::Walking;
  candidate_.result = Result::None;
  candidate_.encounter = Encounter();
  return submit();
}
} }
