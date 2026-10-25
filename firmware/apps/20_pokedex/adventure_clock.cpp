#include "adventure.hpp"
namespace pokedex { namespace adventure {
void Engine::archiveDay(Save& s) {
  if (!s.today.dayKey) return;
  if (s.historySize == kHistoryCount) {
    for (size_t i = 1; i < kHistoryCount; ++i) s.history[i - 1] = s.history[i];
    --s.historySize;
  }
  s.history[s.historySize++] = s.today;
}
bool Engine::applyClock(Save& s, const ClockReading& r) {
  ClockState& c = s.clock;
  const bool wasTrusted = c.trusted;
  if (!c.calibrated || c.pending.active || c.anomaly || !r.trusted ||
      r.bootGeneration != s.bootGeneration || r.calibrationEpoch != c.calibrationEpoch ||
      r.offsetMinutes != c.offsetMinutes) {
    c.trusted = false; s.today.complete = false;
    return wasTrusted;
  }
  if (r.dayKey < c.dayHighWater) {
    c.trusted = false; c.anomaly = true; s.today.complete = false; return true;
  }
  if (c.anchor.trusted && !consistentReading(c.anchor, r, false)) {
    c.trusted = false; c.anomaly = true; s.today.complete = false; return true;
  }
  bool changed = !wasTrusted;
  if (!s.today.dayKey || r.dayKey > c.dayHighWater) {
    const bool complete = wasTrusted && s.today.dayKey != 0;
    archiveDay(s); s.today = DayBucket(); s.today.dayKey = r.dayKey;
    s.today.epoch = c.calibrationEpoch; s.today.complete = complete;
    c.dayHighWater = r.dayKey; changed = true;
  }
  c.trusted = true; c.anchor = r;
  return changed;
}
CommandResult Engine::observeClock(const ClockReading& r) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if (!validReading(r) || r.bootGeneration != state_.bootGeneration) return CommandResult::InvalidInput;
  if ((battle_.view().active && !battle_.view().paused) || throw_.active) return CommandResult::Deferred;
  stage();
  if (applyClock(candidate_, r)) return submit();
  state_ = candidate_; dirty_ = true;
  return CommandResult::Applied;
}
CommandResult Engine::beginClockChange(uint64_t id, ClockChangeMode mode, const ClockReading& target) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  if ((battle_.view().active && !battle_.view().paused) || throw_.active) return CommandResult::Deferred;
  if (!id || uint8_t(mode) > 2 || !target.trusted || !validReading(target) ||
      target.bootGeneration != state_.bootGeneration || target.calibrationEpoch != state_.clock.calibrationEpoch)
    return CommandResult::InvalidInput;
  if (mode == ClockChangeMode::Normal && state_.clock.calibrated && target.offsetMinutes != state_.clock.offsetMinutes)
    return CommandResult::InvalidInput;
  const ClockChangePending& p = state_.clock.pending;
  if (id == p.transactionId) {
    return p.active && p.mode == mode && p.targetUtcSeconds == target.utcSeconds &&
      p.targetOffsetMinutes == target.offsetMinutes && p.targetDayKey == target.dayKey ?
      CommandResult::Duplicate : CommandResult::InvalidInput;
  }
  if (id < p.transactionId || (mode == ClockChangeMode::RepairWrongDate && state_.clock.calibrationEpoch == UINT32_MAX))
    return CommandResult::InvalidInput;
  stage(); ClockChangePending& next = candidate_.clock.pending;
  next.active = true; next.transactionId = id; next.mode = mode;
  next.targetUtcSeconds = target.utcSeconds; next.targetOffsetMinutes = target.offsetMinutes;
  next.targetDayKey = target.dayKey; candidate_.clock.trusted = false;
  clockTransaction_ = id;
  return submit();
}
CommandResult Engine::finishClockChange(uint64_t id, const ClockReading& r) {
  const CommandResult g = guard(); if (g != CommandResult::Applied) return g;
  const ClockChangePending& p = state_.clock.pending;
  if (!p.active || id != p.transactionId || id != clockTransaction_) return CommandResult::InvalidPhase;
  if (!r.trusted || !validReading(r) || r.bootGeneration != state_.bootGeneration ||
      r.calibrationEpoch != state_.clock.calibrationEpoch || r.offsetMinutes != p.targetOffsetMinutes ||
      r.utcSeconds < p.targetUtcSeconds || r.utcSeconds - p.targetUtcSeconds > 5)
    return CommandResult::InvalidInput;
  stage(); ClockState& c = candidate_.clock;
  if (p.mode == ClockChangeMode::RepairWrongDate) {
    candidate_.today.complete = false; archiveDay(candidate_); candidate_.today = DayBucket();
    ++c.calibrationEpoch; c.dayHighWater = 0;
  }
  c.pending.active = false; c.calibrated = true; c.anomaly = false;
  c.offsetMinutes = r.offsetMinutes; c.anchor = ClockReading();
  ClockReading calibrated = r; calibrated.calibrationEpoch = c.calibrationEpoch;
  applyClock(candidate_, calibrated);
  return submit();
}
} }
