#include "adventure.hpp"
#include "encounter_catalog.hpp"
namespace pokedex { namespace adventure {
CommandResult Engine::enqueue(const StepEvent* events, size_t count, uint32_t seed) {
  if (count && !events) return CommandResult::InvalidInput;
  for (size_t i = 0; i < count; ++i) {
    if (!events[i].batchId || events[i].clock.bootGeneration != state_.bootGeneration ||
        !validReading(events[i].clock) || (events[i].partner && !registered(state_, events[i].partner)) || (i && events[i].batchId <= events[i - 1].batchId))
      return CommandResult::InvalidInput;
  }
  bool added = false, full = false;
  uint64_t last = queueSize_ ? queue_[queueSize_ - 1].batchId :
    pending_ ? candidate_.lastStepBatch : state_.lastStepBatch;
  for (size_t i = 0; i < count; ++i) {
    if (events[i].batchId <= last) continue;
    if (queueSize_ == kStepQueueCapacity) { overflow_ = true; full = true; continue; }
    if (!queueSize_) queueSeed_ = seed;
    queue_[queueSize_++] = events[i]; last = events[i].batchId; added = true;
  }
  return full ? CommandResult::BufferFull : added ? CommandResult::Applied : CommandResult::Duplicate;
}
CommandResult Engine::observeSteps(const StepEvent* events, size_t count, uint32_t seed) {
  if (!ready_) return CommandResult::NotReady;
  if (count && !events) return CommandResult::InvalidInput;
  for (size_t i = 0; i < count; ++i) {
    if (!events[i].batchId || events[i].clock.bootGeneration != state_.bootGeneration ||
        !validReading(events[i].clock) || (events[i].partner && !registered(state_, events[i].partner)) || (i && events[i].batchId <= events[i - 1].batchId))
      return CommandResult::InvalidInput;
  }
  if (pending_ || (battle_.view().active && !battle_.view().paused) || throw_.active) {
    const CommandResult r = enqueue(events, count, seed);
    return r == CommandResult::Applied ? (pending_ ? CommandResult::SavePending : CommandResult::Deferred) : r;
  }
  // Process bounded chunks without imposing a 128-step maximum on callers.
  bool changed = false;
  for (size_t i = 0; i < count; ++i) {
    const CommandResult q = enqueue(events + i, 1, seed);
    if (q == CommandResult::InvalidInput || q == CommandResult::BufferFull) return q;
    changed = changed || q == CommandResult::Applied;
    const CommandResult r = flushSteps();
    if (r != CommandResult::Applied && r != CommandResult::Duplicate) {
      const CommandResult tail = enqueue(events + i + 1, count - i - 1, seed);
      return tail == CommandResult::BufferFull ? tail : r;
    }
  }
  return changed ? CommandResult::Applied : CommandResult::Duplicate;
}
void Engine::updateAchievements(Save& s, const ClockReading& r) {
  static const uint64_t daily[] = {1000,3000,5000}, life[] = {10000,50000,100000};
  DayBucket* bucket = nullptr;
  if (r.trusted && r.calibrationEpoch == s.clock.calibrationEpoch && s.clock.trusted) {
    if (r.dayKey == s.today.dayKey) bucket = &s.today;
    for (size_t j = 0; j < s.historySize; ++j)
      if (s.history[j].epoch == r.calibrationEpoch && s.history[j].dayKey == r.dayKey) bucket = &s.history[j];
  }
  for (size_t i = 0; i < 3; ++i) {
    if (bucket && bucket->steps >= daily[i] && !bucket->milestones[i].reached) {
      Achievement a; a.reached = true; a.first = makeStamp(s, r);
      bucket->milestones[i] = a;
      if (!s.dailyFirst[i].reached) s.dailyFirst[i] = a;
    }
    if (s.totalSteps >= life[i] && !s.lifetime[i].reached) {
      s.lifetime[i].reached = true; s.lifetime[i].first = makeStamp(s, r);
    }
  }
}
bool Engine::applyStep(Save& s, const StepEvent& e) {
  if (e.batchId <= s.lastStepBatch) return false;
  s.lastStepBatch = e.batchId;
  if (s.totalSteps == UINT64_MAX || s.explorationCredit == UINT64_MAX ||
      (e.partner && s.progress[e.partner - 1].walked == UINT64_MAX)) {
    s.stepsIncomplete = true; return true;
  }
  ++s.totalSteps; ++s.explorationCredit;
  bool mandatory = (s.totalSteps - s.rewardBaseSteps) % 1000 == 0;
  if (e.partner) {
    SpeciesProgress& progress = s.progress[e.partner - 1]; ++progress.walked;
    size_t count = 0; const EvolutionRule* rules = evolutionRules(count);
    for (size_t i = 0; i < count; ++i)
      if (rules[i].from == e.partner && progress.walked - progress.spent == rules[i].steps) mandatory = true;
  }
  const ClockReading& r = e.clock;
  bool dated = s.clock.calibrated && s.clock.trusted && !s.clock.anomaly && !s.clock.pending.active &&
    r.trusted && r.calibrationEpoch == s.clock.calibrationEpoch && r.offsetMinutes == s.clock.offsetMinutes;
  if (dated && s.clock.anchor.trusted && !consistentReading(s.clock.anchor, r, true)) {
    s.clock.trusted = false; s.clock.anomaly = true; s.today.complete = false;
    dated = false; mandatory = true;
  }
  DayBucket* day = nullptr;
  if (dated) {
    if (r.dayKey > s.clock.dayHighWater) {
      archiveDay(s); s.today = DayBucket(); s.today.dayKey = r.dayKey;
      s.today.epoch = s.clock.calibrationEpoch; s.today.complete = true;
      s.clock.dayHighWater = r.dayKey; mandatory = true;
    }
    if (r.dayKey == s.today.dayKey) day = &s.today;
    for (size_t i = 0; i < s.historySize; ++i)
      if (s.history[i].dayKey == r.dayKey && s.history[i].epoch == r.calibrationEpoch) day = &s.history[i];
  }
  if (day) {
    ++day->steps;
    if (day->steps > s.bestDaySteps) s.bestDaySteps = day->steps;
    mandatory = mandatory || day->steps == 1000 || day->steps == 3000 || day->steps == 5000;
  } else {
    ++s.undatedSteps; s.today.complete = false;
  }
  ClockReading eventClock = r;
  if (!day) eventClock.trusted = false;
  updateAchievements(s, eventClock);
  mandatory = mandatory || s.totalSteps == 10000 || s.totalSteps == 50000 || s.totalSteps == 100000;
  return mandatory;
}
CommandResult Engine::flushSteps() {
  if (pending_) return CommandResult::SavePending;
  if (!queueSize_ && !overflow_) return CommandResult::Duplicate;
  stage(); bool mandatory = overflow_;
  if (overflow_) { candidate_.stepsIncomplete = true; candidate_.today.complete = false; }
  for (size_t i = 0; i < queueSize_; ++i) mandatory = applyStep(candidate_, queue_[i]) || mandatory;
  const Phase oldPhase = candidate_.phase;
  createEncounter(candidate_, queueSeed_);
  mandatory = mandatory || oldPhase != candidate_.phase;
  queueSize_ = 0; overflow_ = false;
  if (mandatory) return submit();
  state_ = candidate_; dirty_ = true;
  return CommandResult::Applied;
}
} }
