#include "runtime.hpp"
namespace pokedex { namespace ui {
using namespace adventure;
ClockReading Runtime::makeReading(const device_clock::Reading& candidate, uint32_t at, int offset, bool transaction) const {
  const auto& s = engine_.view(); ClockReading r;
  r.bootGeneration = s.bootGeneration; r.monotonicMs = at;
  r.calibrationEpoch = s.clock.calibrationEpoch; r.offsetMinutes = offset;
  r.utcSeconds = candidate.utcSeconds;
  calendar::CivilTime local;
  r.trusted = candidate.valid && calendar::fromUtc(r.utcSeconds, offset, local) && calendar::dayKey(local, r.dayKey) &&
              (transaction || (s.clock.calibrated && !s.clock.pending.active && !s.clock.anomaly));
  return r;
}
ClockReading Runtime::readingAt(uint32_t at) const {
  const auto& s = engine_.view(); ClockReading r;
  r.bootGeneration = s.bootGeneration; r.monotonicMs = at;
  r.calibrationEpoch = s.clock.calibrationEpoch; r.offsetMinutes = s.clock.offsetMinutes;
  if (!anchor_.trusted || !s.clock.trusted || s.clock.pending.active || s.clock.anomaly ||
      anchor_.calibrationEpoch != r.calibrationEpoch || anchor_.offsetMinutes != r.offsetMinutes) return r;
  int32_t ms = int32_t(at - anchor_.monotonicMs);
  if (ms < -4000 || ms > 2500) return r;
  // A delayed confirmed peak may precede the newest anchor, within the same epoch.
  r.utcSeconds = anchor_.utcSeconds + (ms >= 0 ? ms / 1000 : (ms - 999) / 1000);
  calendar::CivilTime local;
  r.trusted = calendar::fromUtc(r.utcSeconds, r.offsetMinutes, local) && calendar::dayKey(local, r.dayKey);
  return r;
}
void Runtime::pollClock(uint32_t now) {
  auto candidate = platform_.readClock();
  if (!candidate.valid) ++metrics_.rtcErrors;
  now = platform_.now();
  auto r = makeReading(candidate, now, engine_.view().clock.offsetMinutes);
  const auto previous = engine_.view().clock.anchor;
  if (pendingClockWrite_) r.trusted = false;
  auto result = practice_.view().active && !practice_.view().paused ? CommandResult::Deferred : engine_.observeClock(r);
  if ((result == CommandResult::Deferred || result == CommandResult::SavePending) && previous.trusted && previous.bootGeneration == r.bootGeneration) {
    const int32_t elapsed = int32_t(r.monotonicMs - previous.monotonicMs);
    const int64_t drift = (r.utcSeconds - previous.utcSeconds) * 1000 - elapsed;
    if (elapsed < 0 || drift > 120000 || drift < -120000) r.trusted = false;
  }
  clockDeferred_ = result == CommandResult::Deferred || result == CommandResult::SavePending || result == CommandResult::StorageError;
  lastClock_ = now;
  if (!r.trusted || engine_.view().clock.anomaly || engine_.view().clock.pending.active) {
    if (anchor_.trusted) detector_.reset();
    anchor_ = ClockReading();
  } else if (engine_.view().clock.trusted) anchor_ = r;
  dirty_ = true;
}
void Runtime::openClock() {
  navigate(Page::Clock); screen_.clockError = false;
  screen_.clockMode = ClockChangeMode::Normal;
  screen_.offset = engine_.view().clock.calibrated ? engine_.view().clock.offsetMinutes : 480;
  const auto candidate = platform_.readClock();
  calendar::CivilTime local;
  if (candidate.valid && calendar::fromUtc(candidate.utcSeconds, screen_.offset, local)) screen_.civil = local;
  screen_.civil.second = 0;
}
void Runtime::editClock(int delta) {
  int* field = nullptr; int low = 0, high = 0, step = 1;
  switch (screen_.selected) {
    case 0: field = &screen_.civil.year; low = 2000; high = 2099; break;
    case 1: field = &screen_.civil.month; low = 1; high = 12; break;
    case 2: field = &screen_.civil.day; low = 1; high = 31; break;
    case 3: field = &screen_.civil.hour; high = 23; break;
    case 4: field = &screen_.civil.minute; high = 59; break;
    case 5: field = &screen_.offset; low = -720; high = 840; step = 15; break;
    case 6: screen_.clockMode = ClockChangeMode((int(screen_.clockMode) + delta + 3) % 3); return;
    default: return;
  }
  *field += delta * step;
  if (*field > high) *field = low;
  if (*field < low) *field = high;
  dirty_ = true;
}
void Runtime::saveClock(uint32_t now) {
  int64_t utc;
  if (!calendar::toUtc(screen_.civil, screen_.offset, utc)) { screen_.clockError = true; return; }
  device_clock::Reading candidate = {}; candidate.valid = true; candidate.utcSeconds = utc;
  auto target = makeReading(candidate, now, screen_.offset, true);
  uint64_t id = engine_.view().clock.pending.transactionId;
  if (id == UINT64_MAX) { screen_.clockError = true; return; }
  pendingClockWrite_ = id + 1;
  discardAnchors();
  auto result = engine_.beginClockChange(pendingClockWrite_, screen_.clockMode, target);
  if (result == CommandResult::Applied && !engine_.savePending()) continueClockWrite();
  else if (!engine_.savePending()) { pendingClockWrite_ = 0; screen_.clockError = true; }
}
void Runtime::continueClockWrite() {
  const auto pending = engine_.view().clock.pending;
  if (!pendingClockWrite_ || !pending.active || pending.transactionId != pendingClockWrite_ || engine_.savePending()) return;
  auto candidate = platform_.writeClock(pending.targetUtcSeconds);
  pendingClockWrite_ = 0;
  if (!candidate.valid) {
    ++metrics_.rtcErrors; screen_.clockError = true; navigate(Page::Clock); return;
  }
  auto verified = makeReading(candidate, platform_.now(), pending.targetOffsetMinutes, true);
  auto result = engine_.finishClockChange(pending.transactionId, verified);
  if (result == CommandResult::Applied) { dateSkipped_ = true; navigate(engine_.view().starter ? Page::Home : Page::Starter); }
  else if (!engine_.savePending()) screen_.clockError = true;
  clockDeferred_ = true;
}
} }
