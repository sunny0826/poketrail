#pragma once
#include "harness.hpp"

// Storage failures, uncertain commits, buffered steps and crash recovery.
static void failures() {
  World w; w.steps(4999); w.store.failBefore = true;
  StepEvent event; event.batchId = 5000; event.clock = w.clock();
  CHECK(w.e.observeSteps(&event, 1, 12) == CommandResult::StorageError);
  const std::vector<uint8_t> frozen = w.store.lastAttempt;
  CHECK(w.e.view().totalSteps == 4999 && w.e.view().phase == Phase::Walking);
  CHECK(w.e.selectRoute(Route::Cave) == CommandResult::SavePending);
  StepEvent next[3]; for (unsigned i = 0; i < 3; ++i) { next[i].batchId = 5001 + i; next[i].clock = w.clock(); }
  CHECK(w.e.observeSteps(next, 3, 19) == CommandResult::SavePending);
  CHECK(w.e.observeSteps(next, 3, 19) == CommandResult::Duplicate);
  CHECK(w.e.retrySave() == CommandResult::StorageError && frozen == w.store.lastAttempt);
  w.store.failBefore = false; CHECK(w.e.retrySave() == CommandResult::Applied);
  CHECK(w.e.view().totalSteps == 5003 && w.e.view().explorationCredit == 3);
  CHECK(w.e.checkpoint() == CommandResult::Applied);
  World capture; capture.steps(5000, seedFor(capture.e.view(), true, true)); capture.win();
  capture.store.failAfter = true; CHECK(capture.ball(1) == CommandResult::StorageError);
  CHECK(capture.e.view().recentSize == 0 && capture.e.view().encounter.usedBalls == 0);
  const std::vector<uint8_t> captured = capture.store.disk;
  capture.store.failAfter = false; CHECK(capture.e.retrySave() == CommandResult::Applied);
  CHECK(capture.store.disk == captured && capture.e.view().recentSize == 1);
  Engine restart(capture.store); CHECK(restart.begin() == CommandResult::Applied);
  CHECK(restart.view().recentSize == 1 && restart.view().result == Result::Captured);
  CHECK(restart.pressThrow(AttemptKey(restart.view().encounter.id, 1), 0, reading(restart, 0, 0, false)) == CommandResult::Duplicate);
  World full; full.steps(5000); CHECK(full.e.startBattle(full.now) == CommandResult::Applied);
  StepEvent many[129]; for (size_t i = 0; i < 129; ++i) { many[i].batchId = 5001 + i; many[i].clock = full.clock(); }
  CHECK(full.e.observeSteps(many, 129, 1) == CommandResult::BufferFull && full.e.stepsIncomplete());
  CHECK(full.e.retreat() == CommandResult::Applied);
  CHECK(full.e.view().totalSteps == 5128 && full.e.view().stepsIncomplete);
  for (LoadStatus status : {LoadStatus::Corrupt, LoadStatus::UnsupportedVersion, LoadStatus::IoError, LoadStatus::NeedsProvisioning}) {
    MemoryStore store; store.forced = status; Engine e(store);
    CHECK(e.begin() == CommandResult::StorageError && !e.ready() && store.commits == 0);
  }
  MemoryStore boot; boot.failBefore = true; Engine e(boot);
  CHECK(e.begin() == CommandResult::StorageError && !e.ready());
  boot.failBefore = false; CHECK(e.retrySave() == CommandResult::Applied && e.ready());
  std::puts("PASS failed writes, frozen retry, uncertain commit, replay queue, overflow and load errors");
}
static void additionalRecovery() {
  World w; w.steps(5000, seedFor(w.e.view(), false, true)); w.win();
  CHECK(w.ball(1) == CommandResult::Applied);
  Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  const uint64_t id = reboot.view().encounter.id;
  CHECK(reboot.view().phase == Phase::ThrowReady && reboot.view().encounter.usedBalls == 1);
  CHECK(reboot.beginThrow(AttemptKey(id, 1), 10) == CommandResult::Duplicate);
  CHECK(reboot.beginThrow(AttemptKey(id, 2), 10) == CommandResult::Applied);
  CHECK(reboot.pressThrow(AttemptKey(id, 2), reboot.throwView().targetMs,
        reading(reboot, 0, reboot.throwView().targetMs, false)) == CommandResult::Applied);
  CHECK(reboot.view().recentSize == 1);
  World prize; prize.steps(5000, seedFor(prize.e.view(), true, true)); prize.win();
  prize.store.failBefore = true; CHECK(prize.ball(1) == CommandResult::StorageError);
  prize.store.failBefore = false; Engine powerLoss(prize.store); CHECK(powerLoss.begin() == CommandResult::Applied);
  CHECK(powerLoss.view().phase == Phase::ThrowReady && powerLoss.view().recentSize == 0);
  CHECK(powerLoss.beginThrow(AttemptKey(powerLoss.view().encounter.id, 1), 100) == CommandResult::Applied);
  CHECK(powerLoss.pressThrow(AttemptKey(powerLoss.view().encounter.id, 1), powerLoss.throwView().targetMs,
        reading(powerLoss, 0, 1600, false)) == CommandResult::Applied);
  CHECK(powerLoss.view().result == Result::Captured && powerLoss.view().recentSize == 1);
  World overflow; overflow.steps(4999); overflow.store.failBefore = true;
  StepEvent event; event.batchId = 5000; event.clock = overflow.clock();
  CHECK(overflow.e.observeSteps(&event, 1, 1) == CommandResult::StorageError);
  StepEvent many[129]; for (unsigned i = 0; i < 129; ++i) { many[i].batchId = 5001 + i; many[i].clock = overflow.clock(); }
  CHECK(overflow.e.observeSteps(many, 129, 1) == CommandResult::BufferFull);
  overflow.store.failBefore = false; overflow.store.failAt = overflow.store.commits + 2;
  CHECK(overflow.e.retrySave() == CommandResult::StorageError && overflow.e.stepsIncomplete());
  CHECK(overflow.e.view().totalSteps == 5000 && overflow.e.bufferedSteps() == 0);
  CHECK(overflow.e.retrySave() == CommandResult::Applied);
  CHECK(overflow.e.view().totalSteps == 5128 && overflow.e.stepsIncomplete());
  World life; life.steps(100000);
  for (unsigned i = 0; i < 3; ++i) {
    CHECK(life.e.view().lifetime[i].reached && !life.e.view().lifetime[i].first.timeKnown);
    CHECK(life.e.view().lifetime[i].first.bootGeneration == 1);
  }
  CHECK(life.e.view().lifetime[0].first.totalSteps == 10000 &&
        life.e.view().lifetime[1].first.totalSteps == 50000 && life.e.view().lifetime[2].first.totalSteps == 100000);
  StepEvent invalid[2]; invalid[0].batchId = 100001; invalid[0].clock = life.clock();
  invalid[1] = invalid[0];
  CHECK(life.e.observeSteps(invalid, 2, 1) == CommandResult::InvalidInput && life.e.view().totalSteps == 100000);
  std::puts("PASS saved first failure/second success, prewrite power loss, overflow visibility and lifetime milestones");
}
static void battleCommitRecovery() {
  for (unsigned written = 0; written < 2; ++written) {
    World w; w.steps(5000); CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
    for (;;) {
      w.now = w.e.battleView().targetMs;
      const bool finalHit = w.e.battleView().expected == Action::Attack && w.e.battleView().enemyHp <= 2;
      if (finalHit) { w.store.failBefore = !written; w.store.failAfter = written; }
      const CommandResult r = w.e.press(w.e.battleView().expected, w.now);
      if (finalHit) { CHECK(r == CommandResult::StorageError && w.e.view().phase == Phase::Battle); break; }
      CHECK(r == CommandResult::Applied); w.now += 181; CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
    }
    w.store.failBefore = w.store.failAfter = false;
    Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
    CHECK(reboot.view().phase == (written ? Phase::ThrowReady : Phase::EncounterReady));
    CHECK(reboot.view().encounter.id == 1 && reboot.view().explorationCredit == 0);
  }
  World w; w.steps(10000, seedFor(w.e.view(), true, true)); w.win(); CHECK(w.ball(1) == CommandResult::Applied);
  w.store.failAfter = true; CHECK(w.e.acknowledgeResult(1, 77) == CommandResult::StorageError);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().encounter.id == 1);
  w.store.failAfter = false; Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::EncounterReady && reboot.view().encounter.id == 2 && reboot.view().explorationCredit == 0);
  CHECK(reboot.view().recentSize == 1);
  std::puts("PASS victory and result acknowledgement crash points preserve encounter and credit");
}
