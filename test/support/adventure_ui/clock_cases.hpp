#pragma once
#include "harness.hpp"

namespace {
void clockAndErrors() {
  MemoryStore store; TestPlatform p; Runtime r(store, p); start(r);
  p.durable = &store;
  select(r, p, 0); check(r.screen().page == Page::Clock, "first boot date editor reachable"); published(r, "clock");
  select(r, p, 1);
  for (int i = 0; i < 4; ++i) edge(r, p, false, true);
  edge(r, p, true, true); select(r, p, 2);
  for (int i = 0; i < 9; ++i) edge(r, p, false, true);
  edge(r, p, true, true); select(r, p, 7);
  check(r.screen().clockError && p.clockWrites == 0, "February 31 rejected before any RTC write");
  published(r, "clock-invalid-date");
  select(r, p, 2);
  for (int i = 0; i < 3; ++i) edge(r, p, true, false);
  edge(r, p, true, true);
  select(r, p, 7); check(p.clockWrites == 1 && r.engine().view().clock.calibrated, "manual date persists pending before RTC and confirms");
  select(r, p, 1); check(r.engine().view().starter == 4, "second starter choice works");
  openMenu(r, p); select(r, p, 4); select(r, p, 1); select(r, p, 6);
  edge(r, p, false, true); edge(r, p, false, true); edge(r, p, true, true);
  check(r.screen().clockMode == ClockChangeMode::RepairWrongDate, "explicit repair mode selectable with keys");
  p.writeValid = false; select(r, p, 7);
  check(r.engine().view().clock.pending.active && !r.engine().view().clock.trusted && r.screen().clockError, "failed RTC verification retains pending unknown");
  const unsigned writes = p.clockWrites;
  Runtime restart(store, p); start(restart);
  check(p.clockWrites == writes && restart.screen().page == Page::DateWelcome, "restart pending never rewrites stale RTC target");
  select(restart, p, 0); p.writeValid = true; select(restart, p, 7);
  check(p.clockWrites == writes + 1 && !restart.engine().view().clock.pending.active, "user reconfirms current time using new transaction");
  for (LoadStatus status : {LoadStatus::NeedsProvisioning, LoadStatus::Corrupt, LoadStatus::UnsupportedVersion, LoadStatus::IoError}) {
    MemoryStore bad; bad.status = status; Runtime error(bad, p); start(error);
    check(error.screen().page == Page::Error && bad.commits == 0, "failed load never initializes rewards");
    const char* name = status == LoadStatus::NeedsProvisioning ? "storage-uninitialized" : status == LoadStatus::Corrupt ? "storage-corrupt" : status == LoadStatus::UnsupportedVersion ? "storage-new-schema" : "storage-io";
    published(error, name); select(error, p, 1);
    check(error.screen().page == Page::Viewer && bad.commits == 0, "viewer remains usable with failed storage");
    tap(error, p, 233, 30); check(error.screen().page == Page::Error, "viewer returns to specific storage error");
  }
}
void midnightAndSampling() {
  MemoryStore store; TestPlatform p;
  calendar::CivilTime civil = {2026, 10, 22, 23, 59, 59};
  check(calendar::toUtc(civil, 0, p.utc), "midnight fixture is real calendar time");
  p.rtcSetAt = host_time_ms;
  {
    Engine e(store); e.begin(); e.chooseStarter(1);
    ClockReading target; target.trusted = true; target.utcSeconds = p.utc;
    target.dayKey = 20261022; target.bootGeneration = e.view().bootGeneration;
    target.monotonicMs = host_time_ms;
    check(e.beginClockChange(1, ClockChangeMode::Normal, target) == CommandResult::Applied &&
          e.finishClockChange(1, target) == CommandResult::Applied, "calibrated restart prepared through public commands");
  }
  Runtime r(store, p); start(r); walk(r, p, 3);
  check(r.engine().view().totalSteps == 3 && r.engine().view().historySize == 1 &&
        r.engine().view().history[0].steps == 2 && r.engine().view().today.steps == 1,
        "delayed first three peaks cross midnight into their original day buckets");
  check(r.engine().view().today.dayKey == 20261023 && r.engine().view().explorationCredit == 3,
        "midnight preserves cumulative steps and unused credit");
  r.feedback(25); p.vibrationEnd = host_time_ms + 3000;
  walk(r, p, 6);
  check(r.engine().view().totalSteps == 3, "actual delayed vibration stop suppresses all movement");
  idle(r, 240); walk(r, p, 3);
  check(r.engine().view().totalSteps == 6, "new cadence after actual stop and decay resumes counting");
  const auto dailySteps = r.engine().view().today.steps;
  openMenu(r, p); select(r, p, 4); select(r, p, 1); select(r, p, 7);
  check(r.engine().view().clock.trusted && r.engine().view().today.steps == dailySteps,
        "ordinary same-day correction through UI preserves daily bucket");
  const auto gaps = r.metrics().qualityGaps;
  p.fresh = false; p.imuError = true; idle(r, 200);
  check(r.metrics().qualityGaps > gaps && r.metrics().noSample >= 10 && r.metrics().imuIoErrors >= 10,
        "missing IMU samples and exposed I/O failures are measured without synthetic samples");
  p.fresh = true; p.imuError = false; p.rtcValid = false; idle(r, 1000); walk(r, p, 3);
  check(!r.engine().view().clock.trusted && r.engine().view().undatedSteps == 3,
        "invalid RTC candidate keeps walking but does not fabricate today dates");
}
void practiceMidnightAchievement() {
  MemoryStore store; TestPlatform p;
  {
    Engine e(store);
    check(e.begin() == CommandResult::Applied && e.chooseStarter(1) == CommandResult::Applied,
          "midnight practice fixture boots through public commands");
    ClockReading noon; noon.trusted = true; noon.utcSeconds = 1792670400LL;
    noon.dayKey = 20261022; noon.bootGeneration = e.view().bootGeneration; noon.monotonicMs = 1000;
    check(e.beginClockChange(1, ClockChangeMode::Normal, noon) == CommandResult::Applied &&
          e.finishClockChange(1, noon) == CommandResult::Applied, "practice fixture is calibrated");
    for (unsigned i = 0; i < 9997; ++i) {
      StepEvent event; event.batchId = i + 1; event.clock = noon;
      event.clock.monotonicMs = 2000 + i;
      check(e.observeSteps(&event, 1, 1) == CommandResult::Applied, "dated fixture step accepted");
    }
    check(e.checkpoint() == CommandResult::Applied, "9997 steps persisted before runtime reload");
  }
  p.utc = 1792713595LL; p.rtcSetAt = host_time_ms;
  Runtime r(store, p); start(r); openMenu(r, p); select(r, p, 5);
  check(r.screen().page == Page::Practice, "practice opens before midnight");
  walk(r, p, 6);
  check(r.engine().view().totalSteps == 9997, "practice buffers the six pre-midnight peaks");
  idle(r, 3000); tap(r, p, 233, 30); idle(r, 2000); frame(r);
  const Save& s = r.engine().view();
  check(s.totalSteps == 10003 && s.today.dayKey == 20261023 && s.today.steps == 0 &&
        s.historySize == 1 && s.history[0].dayKey == 20261022 && s.history[0].steps == 10003,
        "practice releases original pre-midnight peaks into their historical day");
  check(s.lifetime[0].reached && s.lifetime[0].first.timeKnown &&
        s.lifetime[0].first.dayKey == 20261022 && s.lifetime[0].first.dailySteps == 10000,
        "practice crossing midnight preserves the exact lifetime achievement date and count");
  Save reloaded;
  check(store.load(reloaded).status == LoadStatus::Loaded && reloaded.lifetime[0].first.timeKnown &&
        reloaded.lifetime[0].first.dayKey == 20261022,
        "practice milestone date survives encoding and reload");
}
}
