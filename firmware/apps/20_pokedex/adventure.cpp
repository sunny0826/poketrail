#include "adventure.hpp"
#include "adventure_migration.hpp"
#include "encounter_catalog.hpp"
#include <limits>
#include <new>
namespace pokedex { namespace adventure {
CommandResult Engine::guard() const {
  return pending_ ? CommandResult::SavePending : !ready_ ? CommandResult::NotReady : CommandResult::Applied;
}
void Engine::stage() { candidate_ = state_; }
CommandResult Engine::begin(bool persistBoot) {
  if (pending_) return CommandResult::SavePending;
  if (ready_) return CommandResult::Duplicate;
  loadResult_ = store_.load(candidate_);
  if (loadResult_.status != LoadStatus::Loaded && loadResult_.status != LoadStatus::Empty)
    return CommandResult::StorageError;
  if (loadResult_.status == LoadStatus::Empty) {
    candidate_.~Save();
    new (&candidate_) Save();
    candidate_.encounterTarget = nextEncounterTarget(mixRandom(candidate_.sequence));
  }
  if (!validate(candidate_) || candidate_.bootGeneration == UINT32_MAX) {
    loadResult_.status = LoadStatus::Corrupt;
    return CommandResult::StorageError;
  }
  if (!migrateLegacy(candidate_)) { loadResult_.status = LoadStatus::Corrupt; return CommandResult::StorageError; }
  if (candidate_.phase != Phase::Walking && candidate_.phase != Phase::Resolved &&
      candidate_.captureCounts[candidate_.encounter.species - 1]) {
    candidate_.phase = Phase::Resolved;
    candidate_.result = Result::Released;
  }
  if (!persistBoot) {
    // Mode switches are not device boots. Load the official slot without the
    // normal boot-generation write so a test session cannot change its bytes.
    state_ = candidate_;
    ready_ = true;
    dirty_ = false;
    pending_ = false;
    bootPending_ = false;
    return CommandResult::Applied;
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
  if (!s.starter) return;
  while (s.nextEncounterId != UINT64_MAX &&
         s.encounterQueueSize < s.encounterQueue.size() &&
         s.encounterTarget >= kEncounterMinSteps && s.explorationCredit >= s.encounterTarget) {
    const uint16_t species = chooseEncounter(s, s.route, seed);
    if (!species) return;
    s.explorationSpent += s.encounterTarget;
    s.explorationCredit -= s.encounterTarget;
    Encounter e;
    e.id = s.nextEncounterId++; e.species = species; e.partner = s.partner;
    e.route = s.route; e.seed = seed;
    if (s.phase == Phase::Walking) {
      s.encounter = e; s.phase = Phase::EncounterReady; s.result = Result::None;
    } else s.encounterQueue[s.encounterQueueSize++] = e;
    s.encounterTarget = nextEncounterTarget(seed ^ uint32_t(s.nextEncounterId));
    seed = mixRandom(seed ^ uint32_t(e.id) ^ uint32_t(e.id >> 32));
  }
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
  return submit();
}
CommandResult Engine::selectRoute(Route route) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!isRegion(route)) return CommandResult::InvalidInput;
  if ((battle_.view().active && !battle_.view().paused) || throw_.active) return CommandResult::Deferred;
  if (state_.route == route) return CommandResult::Duplicate;
  stage(); candidate_.route = route;
  createEncounter(candidate_, mixRandom(state_.bootGeneration ^ uint32_t(state_.nextEncounterId)));
  return submit();
}
CommandResult Engine::selectLanguage(i18n::Language language) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!i18n::validLanguage(language)) return CommandResult::InvalidInput;
  if (state_.clock.pending.active ||
      (battle_.view().active && !battle_.view().paused) || throw_.active)
    return CommandResult::Deferred;
  if (state_.language == language) return CommandResult::Duplicate;
  stage(); candidate_.language = language;
  return submit();
}
CommandResult Engine::startBattle(uint32_t now) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::EncounterReady || !registered(state_, state_.encounter.partner)) return CommandResult::InvalidPhase;
  state_.phase = Phase::Battle; dirty_ = true;
  const CatalogEntry* entry = catalogEntry(state_.encounter.species);
  battle_.begin(now, state_.encounter.seed, entry && entry->rare);
  return CommandResult::Applied;
}
CommandResult Engine::finishBattle() {
  if (battle_.view().result == Result::None) return CommandResult::Applied;
  stage(); candidate_.result = battle_.view().result;
  candidate_.phase = candidate_.result == Result::Won ? Phase::ThrowReady : Phase::Resolved;
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
CommandResult Engine::acknowledgeResult(uint64_t id, uint32_t /* seed */) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (state_.phase != Phase::Resolved) return CommandResult::InvalidPhase;
  if (id != state_.encounter.id) return CommandResult::InvalidInput;
  stage(); candidate_.phase = Phase::Walking; candidate_.result = Result::None;
  candidate_.encounter = Encounter();
  if (candidate_.encounterQueueSize) {
    candidate_.encounter = candidate_.encounterQueue[0];
    for (size_t i = 1; i < candidate_.encounterQueueSize; ++i)
      candidate_.encounterQueue[i - 1] = candidate_.encounterQueue[i];
    candidate_.encounterQueue[--candidate_.encounterQueueSize] = Encounter();
    candidate_.phase = Phase::EncounterReady;
  }
  // Confirming a result only consumes the finished encounter and advances FIFO.
  // Keep banked steps for the next walk or region change, so a stationary player
  // can work through the queue without immediately refilling each freed slot.
  return submit();
}
} }
