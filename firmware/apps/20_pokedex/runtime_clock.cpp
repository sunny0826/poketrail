#include "runtime.hpp"
#include "calendar.hpp"
namespace pokedex { namespace ui {
using namespace adventure;
namespace {
// Steps are dated from the newest trusted clock anchor, which is refreshed by a
// 1 s clock poll. A batch released by a multi-peak confirmation can predate that
// anchor by the detector's worst-case confirmation delay plus one poll interval.
constexpr int32_t kPastAnchorMs = int32_t(motion::kMaxConfirmationDelayMs) + 1000;
}
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
  if (ms < -kPastAnchorMs || ms > 2500) return r;
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
  const bool practiceBusy = practiceCapture_.stage() == PracticeStage::Battle &&
                            !practiceCapture_.battleView().paused;
  auto result = practiceBusy ? CommandResult::Deferred : engine_.observeClock(r);
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
  if (engine_.savePending()) return;
  int64_t utc;
  if (!calendar::toUtc(screen_.civil, screen_.offset, utc)) { screen_.clockError = true; return; }
  device_clock::Reading candidate = {}; candidate.valid = true; candidate.utcSeconds = utc;
  auto target = makeReading(candidate, now, screen_.offset, true);
  uint64_t id = engine_.view().clock.pending.transactionId;
  if (id == UINT64_MAX) { screen_.clockError = true; return; }
  clearHostClock();
  pendingClockWrite_ = id + 1;
  discardAnchors();
  auto result = engine_.beginClockChange(pendingClockWrite_, screen_.clockMode, target);
  if (result == CommandResult::Applied && !engine_.savePending()) continueClockWrite();
  else if (!engine_.savePending()) { pendingClockWrite_ = 0; screen_.clockError = true; }
}
void Runtime::continueClockWrite() {
  if (hostClockStage_ != HostClockStage::None) {
    resumeHostClock();
    if (!engine_.savePending()) {
      pollClock(platform_.now());
      const auto status = clockSyncStatus();
      if (status.trusted && !status.pending && !status.anomaly) {
        dateSkipped_ = true;
        navigate(engine_.view().starter ? Page::Home : Page::Starter);
      } else {
        openClock();
        screen_.clockError = true;
      }
    }
    return;
  }
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
clock_sync::Status Runtime::clockStatus(const device_clock::Reading& reading) const {
  const auto& s = engine_.view();
  clock_sync::Status status;
  status.testMode = testHost_ && testHost_->testMode();
  status.ready = engine_.ready();
  status.busy = !safe();
  status.savePending = engine_.savePending();
  status.rtcValid = reading.valid;
  status.rtcLowVoltage = reading.lowVoltage;
  status.rtcIoError = reading.ioError;
  status.utcSeconds = reading.valid ? reading.utcSeconds : 0;
  status.offsetMinutes = s.clock.offsetMinutes;
  status.calibrated = s.clock.calibrated;
  status.trusted = reading.valid && s.clock.trusted;
  status.pending = s.clock.pending.active;
  status.anomaly = s.clock.anomaly;
  status.transactionId = s.clock.pending.transactionId;
  status.sequence = s.sequence;
  calendar::CivilTime local;
  uint32_t dayKey = 0;
  if (reading.valid && calendar::fromUtc(reading.utcSeconds, s.clock.offsetMinutes, local) &&
      calendar::dayKey(local, dayKey)) status.dayKey = dayKey;
  return status;
}
clock_sync::Status Runtime::clockSyncStatus() { return clockStatus(platform_.readClock()); }
bool Runtime::hostClockVerified(int64_t targetUtc, int16_t offset) {
  const auto reading = platform_.readClock();
  const auto status = clockStatus(reading);
  return reading.valid && status.calibrated && status.trusted && !status.pending &&
         !status.anomaly && status.offsetMinutes == offset &&
         reading.utcSeconds >= targetUtc && reading.utcSeconds - targetUtc <= 5;
}
void Runtime::clearHostClock() {
  if (pendingClockWrite_ == hostClockId_) pendingClockWrite_ = 0;
  hostClockStage_ = HostClockStage::None;
  hostClockId_ = 0;
  hostClockTargetUtc_ = 0;
  hostClockReceivedAt_ = 0;
  hostClockOffset_ = 0;
  hostClockWriteAttempted_ = false;
  clockDeferred_ = true;
}
const char* Runtime::resumeHostClock() {
  if (hostClockStage_ == HostClockStage::BeginSave) {
    CommandResult result = CommandResult::Applied;
    if (engine_.savePending()) result = engine_.retrySave();
    if (engine_.savePending()) return "save";
    if (result != CommandResult::Applied && result != CommandResult::Duplicate) {
      clearHostClock();
      return result == CommandResult::Deferred ? "busy" : "save";
    }
    const auto& pending = engine_.view().clock.pending;
    if (pending.active && pending.transactionId == hostClockId_) {
      hostClockStage_ = HostClockStage::RtcWrite;
      hostClockWriteAttempted_ = false;
    } else if (!pending.active && pending.transactionId == hostClockId_) {
      const bool ok = hostClockVerified(hostClockTargetUtc_, hostClockOffset_);
      clearHostClock();
      return ok ? "ok" : "stale";
    } else {
      clearHostClock();
      return "stale";
    }
  }
  if (hostClockStage_ == HostClockStage::RtcWrite) {
    if (uint32_t(platform_.now() - hostClockReceivedAt_) > 5000) {
      clearHostClock();
      return "stale";
    }
    const int64_t target = hostClockTargetUtc_;
    const int16_t offset = hostClockOffset_;
    auto current = platform_.readClock();
    bool landed = current.valid && current.utcSeconds >= target && current.utcSeconds - target <= 5;
    if (!landed && hostClockWriteAttempted_) {
      // A retry must never move the clock back to a stale target.
      clearHostClock();
      return "stale";
    }
    if (!landed) {
      hostClockWriteAttempted_ = true;
      current = platform_.writeClock(target);
      if (!current.valid || current.utcSeconds < target || current.utcSeconds - target > 5) {
        ++metrics_.rtcErrors;
        return "rtc";
      }
    }
    const auto verified = makeReading(current, platform_.now(), offset, true);
    const auto result = engine_.finishClockChange(hostClockId_, verified);
    if (result == CommandResult::Applied) {
      const bool ok = hostClockVerified(target, offset);
      clearHostClock();
      if (!ok) return "verify";
      dateSkipped_ = true;
      if (screen_.page == Page::DateWelcome || screen_.page == Page::Clock)
        navigate(engine_.view().starter ? Page::Home : Page::Starter);
      return "ok";
    }
    if (engine_.savePending()) {
      hostClockStage_ = HostClockStage::ConfirmSave;
      return "save";
    }
    clearHostClock();
    return "verify";
  }
  if (hostClockStage_ == HostClockStage::ConfirmSave) {
    const int64_t target = hostClockTargetUtc_;
    const int16_t offset = hostClockOffset_;
    const auto result = engine_.retrySave();
    if (engine_.savePending()) return "save";
    if (result != CommandResult::Applied && result != CommandResult::Duplicate) {
      clearHostClock();
      return "save";
    }
    const bool ok = hostClockVerified(target, offset);
    clearHostClock();
    if (!ok) return "verify";
    dateSkipped_ = true;
    if (screen_.page == Page::DateWelcome || screen_.page == Page::Clock)
      navigate(engine_.view().starter ? Page::Home : Page::Starter);
    return "ok";
  }
  clearHostClock();
  return "stale";
}
const char* Runtime::syncClockFromHost(uint32_t id, int64_t utcSeconds, int16_t offsetMinutes) {
  if (!id) return "id";
  if (testHost_ && testHost_->testMode()) return "test";
  if (!engine_.ready()) return "notready";

  const bool same = hostClockStage_ != HostClockStage::None && hostClockId_ == id;
  if (same) {
    if (pendingClockWrite_ != id ||
        (engine_.view().clock.pending.active && engine_.view().clock.pending.transactionId != id)) {
      clearHostClock();
      return "stale";
    }
    if (utcSeconds != hostClockTargetUtc_ || offsetMinutes != hostClockOffset_) return "id";
    return resumeHostClock();
  }

  if (engine_.savePending()) return "save";
  // A host transaction without a pending commit may be superseded by a fresh
  // id. A manual transaction still in flight must not be hijacked.
  const bool ownHostTransaction = hostClockStage_ != HostClockStage::None &&
                                  pendingClockWrite_ == hostClockId_;
  if (pendingClockWrite_ && !ownHostTransaction) return "pending";

  const auto& clock = engine_.view().clock;
  const auto& pending = clock.pending;
  if (clock.calibrated && offsetMinutes != clock.offsetMinutes) return "offset";
  const int16_t offset = clock.calibrated ? clock.offsetMinutes : offsetMinutes;
  if (!pending.active && pending.transactionId == id) {
    if (pending.targetUtcSeconds != utcSeconds || pending.targetOffsetMinutes != offset) return "id";
    return hostClockVerified(utcSeconds, offset) ? "ok" : "stale";
  }
  if (pending.active && pending.transactionId == id) return "stale";
  if (id <= pending.transactionId) return "id";
  if (!safe()) return "busy";

  // An older host attempt without a pending commit may be superseded.
  if (hostClockStage_ != HostClockStage::None) clearHostClock();

  device_clock::Reading candidate = {};
  candidate.valid = true;
  candidate.utcSeconds = utcSeconds;
  auto target = makeReading(candidate, platform_.now(), offset, true);
  if (!target.trusted) return "range";

  hostClockStage_ = HostClockStage::BeginSave;
  hostClockId_ = id;
  hostClockTargetUtc_ = utcSeconds;
  hostClockReceivedAt_ = platform_.now();
  hostClockOffset_ = offset;
  hostClockWriteAttempted_ = false;
  pendingClockWrite_ = id;
  discardAnchors();
  const auto result = engine_.beginClockChange(id, ClockChangeMode::Normal, target);
  if (engine_.savePending()) return "save";
  if (result != CommandResult::Applied && result != CommandResult::Duplicate) {
    clearHostClock();
    if (result == CommandResult::Deferred) return "busy";
    if (result == CommandResult::NotReady) return "notready";
    return "id";
  }
  hostClockStage_ = HostClockStage::RtcWrite;
  hostClockWriteAttempted_ = false;
  return resumeHostClock();
}
} }
