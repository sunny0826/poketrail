#include "adventure.hpp"
#include "encounter_catalog.hpp"
namespace pokedex { namespace adventure {
CommandResult Engine::beginThrow(AttemptKey key, uint32_t now) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!key.encounterId || key.encounterId != state_.encounter.id || key.attemptIndex < 1 || key.attemptIndex > 2)
    return CommandResult::InvalidInput;
  if (key.attemptIndex <= state_.encounter.usedBalls) return CommandResult::Duplicate;
  if (state_.phase != Phase::ThrowReady || state_.captureCounts[state_.encounter.species - 1])
    return CommandResult::InvalidPhase;
  if (key.attemptIndex != state_.encounter.usedBalls + 1) return CommandResult::InvalidInput;
  if (throw_.active) return CommandResult::Duplicate;
  if (state_.encounter.ruleVersion < 3 && !availableBalls(state_)) return CommandResult::NoBalls;
  throw_.active = true; throw_.key = key; throw_.targetMs = now + 1500;
  return CommandResult::Applied;
}
void Engine::awardCapture(Save& s, const ClockReading& r) {
  const uint16_t id = s.encounter.species;
  Capture capture; capture.species = id; capture.route = s.encounter.route; capture.at = makeStamp(s, r);
  for (size_t i = 0; i < 3; ++i) {
    if (s.today.milestones[i].reached && capture.at.timeKnown) capture.achievementMask |= uint8_t(1u << i);
    if (s.lifetime[i].reached) capture.achievementMask |= uint8_t(1u << (i + 3));
  }
  if (!s.captureCounts[id - 1]) s.firstCapture[id - 1] = capture;
  if (!owns(s, id)) ++s.rosterRevision;
  ++s.captureCounts[id - 1]; setOwned(s, id); setRegistered(s, id);
  if (s.recentSize == kRecentCount) {
    for (size_t i = 1; i < kRecentCount; ++i) s.recent[i - 1] = s.recent[i];
    --s.recentSize;
  }
  s.recent[s.recentSize++] = capture;
}
CommandResult Engine::resolveThrow(AttemptKey key, Timing timing, const ClockReading& r) {
  if (!validReading(r) || r.bootGeneration != state_.bootGeneration) return CommandResult::InvalidInput;
  if (state_.captureCounts[state_.encounter.species - 1] == UINT32_MAX) return CommandResult::InvalidInput;
  if (state_.encounter.ruleVersion < 3 && !availableBalls(state_)) return CommandResult::NoBalls;
  const uint16_t rule = state_.encounter.ruleVersion;
  const bool captured = throwRoll(state_.encounter, key.attemptIndex, rule) <
    captureProbability(rule, catalogEntry(state_.encounter.species)->rare, timing);
  if (captured && !owns(state_, state_.encounter.species) && state_.rosterRevision == UINT64_MAX)
    return CommandResult::InvalidInput;
  // Closing the timeline precedes any synchronous flash work.
  throw_.active = false;
  stage();
  if (overflow_) { candidate_.stepsIncomplete = true; candidate_.today.complete = false; }
  for (size_t i = 0; i < queueSize_; ++i) applyStep(candidate_, queue_[i]);
  queueSize_ = 0; overflow_ = false;
  applyClock(candidate_, r);
  ThrowAttempt& a = candidate_.encounter.attempts[key.attemptIndex - 1];
  if (rule < 3) ++candidate_.ballsSpent;
  a.used = true; a.timing = timing; a.ruleVersion = rule;
  a.probability = captureProbability(rule, catalogEntry(candidate_.encounter.species)->rare, timing);
  a.roll = throwRoll(candidate_.encounter, key.attemptIndex, rule);
  a.captured = a.roll < a.probability;
  candidate_.encounter.usedBalls = key.attemptIndex;
  if (a.captured) {
    awardCapture(candidate_, r); candidate_.phase = Phase::Resolved; candidate_.result = Result::Captured;
  } else if (key.attemptIndex == 2) {
    candidate_.phase = Phase::Resolved; candidate_.result = Result::Escaped;
  }
  return submit();
}
CommandResult Engine::pressThrow(AttemptKey key, uint32_t now, const ClockReading& r) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!key.encounterId || key.encounterId != state_.encounter.id || key.attemptIndex < 1 || key.attemptIndex > 2)
    return CommandResult::InvalidInput;
  if (key.attemptIndex <= state_.encounter.usedBalls) return CommandResult::Duplicate;
  if (state_.phase != Phase::ThrowReady || !throw_.active) return CommandResult::InvalidPhase;
  if (key.attemptIndex != throw_.key.attemptIndex) return CommandResult::InvalidInput;
  return resolveThrow(key, throwTiming(now, throw_.targetMs), r);
}
} }
