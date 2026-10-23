#pragma once
#include "harness.hpp"

static void dates() {
  World w(true); w.steps(10); const uint32_t day = w.e.view().today.dayKey;
  const uint64_t total = w.e.view().totalSteps;
  w.calibrate(); CHECK(w.e.view().today.steps == 10);
  CHECK(w.e.observeClock(w.clock(false)) == CommandResult::Applied);
  w.steps(2, 1, false); CHECK(w.e.view().undatedSteps == 2);
  CHECK(w.e.observeClock(w.clock()) == CommandResult::Applied);
  CHECK(w.e.view().today.steps == 10 && !w.e.view().today.complete);
  w.steps(1); CHECK(w.e.view().today.steps == 11);
  ClockReading old = w.clock();
  w.now += 86400000u; CHECK(w.e.observeClock(w.clock()) == CommandResult::Applied);
  CHECK(w.e.view().today.dayKey > day && w.e.view().today.steps == 0);
  CHECK(w.e.view().history[0].steps == 11 && w.e.view().totalSteps == total + 3);
  StepEvent delayed; delayed.batchId = w.e.view().lastStepBatch + 1; delayed.clock = old;
  CHECK(w.e.observeSteps(&delayed, 1, 1) == CommandResult::Applied);
  CHECK(w.e.view().history[0].steps == 12 && w.e.view().today.steps == 0);
  CHECK(w.e.observeClock(old) == CommandResult::Applied && !w.e.view().clock.trusted);
  const uint32_t epoch = w.e.view().clock.calibrationEpoch;
  const uint64_t tx = w.e.view().clock.pending.transactionId + 1;
  ClockReading repaired = old; repaired.monotonicMs = w.now;
  CHECK(w.e.beginClockChange(tx, ClockChangeMode::RepairWrongDate, repaired) == CommandResult::Applied);
  CHECK(w.e.finishClockChange(tx, repaired) == CommandResult::Applied);
  CHECK(w.e.view().clock.calibrationEpoch == epoch + 1 && w.e.view().today.dayKey == day);
  CHECK(w.e.view().historySize == 2 && !w.e.view().history[1].complete);
  w.base -= 86400; w.steps(1); CHECK(w.e.view().today.steps == 1);
  for (unsigned i = 0; i < 16; ++i) {
    w.now += 86400000u; CHECK(w.e.observeClock(w.clock()) == CommandResult::Applied); w.steps(1);
  }
  CHECK(w.e.view().historySize == 14 && w.e.view().undatedSteps == 2);
  World jump(true); ClockReading broken = jump.clock(); broken.utcSeconds += 121;
  CHECK(jump.e.observeClock(broken) == CommandResult::Applied && jump.e.view().clock.anomaly);
  CHECK(jump.e.observeClock(jump.clock()) == CommandResult::Applied && !jump.e.view().clock.trusted);
  jump.calibrate(); CHECK(jump.e.view().clock.trusted);
  World timezone(true); const uint32_t high = timezone.e.view().clock.dayHighWater;
  timezone.calibrate(ClockChangeMode::TimezoneChange, -720);
  CHECK(timezone.e.view().clock.dayHighWater == high && !timezone.e.view().clock.trusted);
  std::puts("PASS static midnight, delayed peaks, unknown, same-day calibration, rollback, repair and history14");
}
static void clockTransactions() {
  World w(true); ClockReading target = w.clock(); target.utcSeconds += 5;
  const uint64_t id = 2;
  w.store.failBefore = true;
  CHECK(w.e.beginClockChange(id, ClockChangeMode::Normal, target) == CommandResult::StorageError);
  CHECK(w.e.finishClockChange(id, target) == CommandResult::SavePending);
  w.store.failBefore = false; CHECK(w.e.retrySave() == CommandResult::Applied);
  CHECK(w.e.view().clock.pending.active && !w.e.view().clock.trusted);
  // Hardware failure means no finish call; restart retains pending and requires a new confirmation.
  Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  ClockReading fresh = reading(reboot, target.utcSeconds, 1000);
  CHECK(reboot.view().clock.pending.active && !reboot.view().clock.trusted);
  CHECK(reboot.finishClockChange(id, fresh) == CommandResult::InvalidPhase);
  CHECK(reboot.beginClockChange(3, ClockChangeMode::Normal, fresh) == CommandResult::Applied);
  w.store.failBefore = true;
  CHECK(reboot.finishClockChange(3, fresh) == CommandResult::StorageError);
  CHECK(reboot.view().clock.pending.active);
  w.store.failBefore = false; CHECK(reboot.retrySave() == CommandResult::Applied);
  CHECK(!reboot.view().clock.pending.active && reboot.view().clock.trusted);
  ClockReading next = reading(reboot, fresh.utcSeconds + 5, 2000);
  CHECK(reboot.beginClockChange(4, ClockChangeMode::Normal, next) == CommandResult::Applied);
  w.store.failAfter = true; CHECK(reboot.finishClockChange(4, next) == CommandResult::StorageError);
  w.store.failAfter = false; Engine finalBoot(w.store); CHECK(finalBoot.begin() == CommandResult::Applied);
  CHECK(!finalBoot.view().clock.pending.active);
  ClockReading later = reading(finalBoot, next.utcSeconds + 86400, 10);
  CHECK(finalBoot.observeClock(later) == CommandResult::Applied && finalBoot.view().clock.trusted);
  std::puts("PASS RTC transaction begin/hardware/finish failures, uncertain finish and reboot confirmation");
}
