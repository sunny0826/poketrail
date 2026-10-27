#include "support/adventure_test/harness.hpp"
#include "support/adventure_test/clock_cases.hpp"

static void basicSteps() {
  World w;
  CHECK(w.e.chooseStarter(4) == CommandResult::InvalidPhase);
  CHECK(w.e.chooseStarter(1) == CommandResult::Duplicate);
  CHECK(w.e.selectPartner(151) == CommandResult::InvalidInput);
  CHECK(w.e.view().captureCounts[0] == 0 && w.e.view().firstCapture[0].species == 0);
  const uint16_t firstTarget = w.e.view().encounterTarget;
  CHECK(firstTarget >= kEncounterMinSteps && firstTarget <= kEncounterMaxSteps);
  w.steps(firstTarget - 1); CHECK(w.e.view().phase == Phase::Walking);
  w.steps(1); CHECK(w.e.view().phase == Phase::EncounterReady && w.e.view().explorationCredit == 0);
  CHECK(w.e.view().explorationSpent == firstTarget);
  const uint16_t nextTarget = w.e.view().encounterTarget;
  CHECK(nextTarget >= kEncounterMinSteps && nextTarget <= kEncounterMaxSteps);
  const Encounter target = w.e.view().encounter;
  w.steps(11000); CHECK(w.e.view().totalSteps == unsigned(firstTarget) + 11000 && w.e.view().explorationCredit == 11000);
  CHECK(w.e.view().undatedSteps == unsigned(firstTarget) + 11000 && w.e.view().today.steps == 0);
  CHECK(w.e.view().encounterTarget == nextTarget);
  CHECK(w.e.selectRoute(Route::Coast) == CommandResult::Applied);
  CHECK(w.e.view().encounter.species == target.species && w.e.view().encounter.route == target.route);
  StepEvent replay; replay.batchId = w.e.view().totalSteps; replay.clock = w.clock();
  CHECK(w.e.observeSteps(&replay, 1, 9) == CommandResult::Duplicate);
  CHECK(w.e.retreat() == CommandResult::Applied);
  const uint64_t spentBefore = w.e.view().explorationSpent;
  CHECK(w.e.acknowledgeResult(target.id, 123) == CommandResult::Applied);
  CHECK(w.e.view().explorationSpent == spentBefore + nextTarget);
  CHECK(w.e.view().explorationCredit == 11000 - nextTarget && w.e.view().encounter.id == 2 && w.e.view().encounter.route == Route::Coast);
  CHECK(w.e.pressThrow(AttemptKey(target.id, 1), 0, w.clock()) == CommandResult::InvalidInput);
  CHECK(w.e.checkpoint() == CommandResult::Duplicate);
  Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().bootGeneration == 2 && reboot.view().lastStepBatch == 0);
  StepEvent fresh; fresh.batchId = 1; fresh.clock = reading(reboot, 0, 10, false);
  const uint64_t total = w.e.view().totalSteps;
  CHECK(reboot.observeSteps(&fresh, 1, 9) == CommandResult::Applied && reboot.view().totalSteps == total + 1);
  CHECK(reboot.observeSteps(&replay, 1, 9) == CommandResult::InvalidInput);
  std::puts("PASS random encounter target, route lock, ownership, replay and boot generation");
}
static void battles() {
  Battle b; b.begin(UINT32_MAX - 1000, 0);
  CHECK(b.view().targetMs == 499);
  CHECK(b.press(Action::Both, 499)); CHECK(b.view().enemyHp == 3);
  CHECK(b.view().settledMs == 499);
  CHECK(!b.press(Action::Attack, 500));
  CHECK(b.requestPause()); b.tick(680); CHECK(b.view().paused && b.view().cueIndex == 1);
  CHECK(b.resume(10000)); CHECK(b.view().targetMs == 11500 && 10000u - b.view().settledMs >= 450);
  CHECK(b.requestPause()); b.tick(11681); CHECK(b.view().playerHp == 2 && b.view().paused);
  CHECK(b.view().settledMs == 11681);
  CHECK(b.resume(20000)); b.tick(100000); CHECK(b.view().result == Result::Lost && b.view().settledMs == 100000);
  b.begin(UINT32_MAX - 1499); CHECK(b.press(Action::Attack, 0)); CHECK(b.view().settledMs == 0);
  CHECK(b.view().enemyHp == 1); b.tick(181); CHECK(b.view().settledMs == 0 && b.view().cueIndex == 1);
  b.begin(0); CHECK(!b.press(Action::Dodge, 1700)); CHECK(b.view().cueIndex == 1 && !b.view().consumed);
  CHECK(b.view().settledMs == 1700);
  b.begin(0);
  for (unsigned i = 0; i < 12; ++i) {
    const uint32_t target = b.view().targetMs;
    if (i % 2) CHECK(b.press(Action::Dodge, target));
    b.tick(target + 181);
  }
  CHECK(b.view().result == Result::Lost && b.view().playerHp == 3);
  CHECK(battleTiming(1180, 1000) == Timing::Normal && battleTiming(1181, 1000) == Timing::Miss);
  CHECK(battleTiming(930, 1000) == Timing::Perfect && battleTiming(929, 1000) == Timing::Normal);
  World w; w.walkToEncounter(); CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  w.now += 30000; CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Lost);
  CHECK(w.e.beginThrow(AttemptKey(w.e.view().encounter.id, 1), w.now) == CommandResult::InvalidPhase);
  World rebooted; rebooted.walkToEncounter(); const uint64_t id = rebooted.e.view().encounter.id;
  CHECK(rebooted.e.startBattle(100) == CommandResult::Applied);
  CHECK(rebooted.e.requestPause() == CommandResult::Applied);
  CHECK(rebooted.e.tick(5000, rebooted.clock()) == CommandResult::Applied);
  CHECK(rebooted.e.checkpoint() == CommandResult::Applied);
  Engine restore(rebooted.store); CHECK(restore.begin() == CommandResult::Applied);
  CHECK(restore.view().phase == Phase::EncounterReady && restore.view().encounter.id == id && restore.view().explorationCredit == 0);
  std::puts("PASS battle first input, wrong/both, wraparound, pause, 6 rounds, lose and reboot");
}
static void throwsAndRecords() {
  World w(true); w.walkToEncounter(seedFor(w.e.view(), false, true)); w.win();
  const uint64_t id = w.e.view().encounter.id;
  CHECK(w.e.beginThrow(AttemptKey(id, 2), w.now) == CommandResult::InvalidInput);
  CHECK(w.ball(1) == CommandResult::Applied); CHECK(w.e.view().phase == Phase::ThrowReady);
  CHECK(w.e.pressThrow(AttemptKey(id, 1), 0, w.clock()) == CommandResult::Duplicate);
  CHECK(w.e.view().encounter.usedBalls == 1);
  CHECK(w.ball(2) == CommandResult::Applied); CHECK(w.e.view().result == Result::Captured);
  const uint16_t species = w.e.view().encounter.species;
  CHECK(w.e.view().captureCounts[species - 1] == 1 && w.e.view().firstCapture[species - 1].at.timeKnown);
  CHECK(w.e.pressThrow(AttemptKey(id, 2), 0, w.clock()) == CommandResult::Duplicate);
  CHECK(w.e.view().captureCounts[species - 1] == 1);
  const Stamp first = w.e.view().firstCapture[species - 1].at;
  CHECK(w.e.selectPartner(species) == CommandResult::InvalidPhase);
  for (unsigned n = 0; n < 33; ++n) {
    const uint32_t seed = seedFor(w.e.view(), true, false);
    CHECK(w.e.acknowledgeResult(w.e.view().encounter.id, seed) == CommandResult::Applied);
    w.steps(1000, seed); w.walkToEncounter(seed); w.win(); CHECK(w.ball(1) == CommandResult::Applied);
    CHECK(w.e.beginThrow(AttemptKey(w.e.view().encounter.id, 2), w.now) == CommandResult::InvalidPhase);
  }
  CHECK(w.e.view().recentSize == 32 && w.e.view().captureCounts[species - 1] == 1);
  unsigned totalCaptures = 0;
  for (auto count : w.e.view().captureCounts) { CHECK(count <= 1); totalCaptures += count; }
  CHECK(totalCaptures == 34);
  CHECK(w.e.view().firstCapture[species - 1].at.utcSeconds == first.utcSeconds);
  CHECK(w.e.view().firstCapture[species - 1].at.totalSteps == first.totalSteps);
  CHECK(w.e.view().lifetime[0].reached && w.e.view().dailyFirst[2].reached);
  World miss; miss.walkToEncounter(); miss.win();
  CHECK(miss.ball(1, Timing::Miss) == CommandResult::Applied);
  CHECK(miss.e.beginThrow(AttemptKey(miss.e.view().encounter.id, 2), miss.now) == CommandResult::Applied);
  miss.now = miss.e.throwView().targetMs + 181;
  CHECK(miss.e.tick(miss.now, miss.clock(false)) == CommandResult::Applied);
  CHECK(miss.e.view().result == Result::Escaped && miss.e.view().recentSize == 0);
  World unknown; unknown.walkToEncounter(seedFor(unknown.e.view(), true, true, 1)); unknown.win();
  CHECK(unknown.ball(1) == CommandResult::Applied);
  CHECK(unknown.e.view().captureCounts[0] == 1 && !unknown.e.view().firstCapture[0].at.timeKnown);
  CHECK(unknown.e.view().firstCapture[0].at.bootGeneration == unknown.e.view().bootGeneration);
  CHECK(unknown.e.view().firstCapture[0].at.monotonicMs == unknown.now);
  unknown.calibrate(); CHECK(!unknown.e.view().firstCapture[0].at.timeKnown);
  CHECK(throwTiming(1120, 1000) == Timing::Nice && throwTiming(1121, 1000) == Timing::Normal);
  std::puts("PASS two throws, deduplication, capture atomic fields, permanent first and recent32");
}
static void failures() {
  World w; const uint16_t target = w.e.view().encounterTarget;
  w.steps(target - 1); w.store.failBefore = true;
  StepEvent event; event.batchId = target; event.clock = w.clock();
  CHECK(w.e.observeSteps(&event, 1, 12) == CommandResult::StorageError);
  const std::vector<uint8_t> frozen = w.store.lastAttempt;
  CHECK(w.e.view().totalSteps == target - 1 && w.e.view().phase == Phase::Walking);
  CHECK(w.e.selectRoute(Route::Cave) == CommandResult::SavePending);
  StepEvent next[3]; for (unsigned i = 0; i < 3; ++i) { next[i].batchId = target + 1 + i; next[i].clock = w.clock(); }
  CHECK(w.e.observeSteps(next, 3, 19) == CommandResult::SavePending);
  CHECK(w.e.observeSteps(next, 3, 19) == CommandResult::Duplicate);
  CHECK(w.e.retrySave() == CommandResult::StorageError && frozen == w.store.lastAttempt);
  w.store.failBefore = false; CHECK(w.e.retrySave() == CommandResult::Applied);
  CHECK(w.e.view().totalSteps == target + 3 && w.e.view().explorationCredit == 3);
  CHECK(w.e.checkpoint() == CommandResult::Applied);
  World capture; capture.walkToEncounter(seedFor(capture.e.view(), true, true)); capture.win();
  capture.store.failAfter = true; CHECK(capture.ball(1) == CommandResult::StorageError);
  CHECK(capture.e.view().recentSize == 0 && capture.e.view().encounter.usedBalls == 0);
  const std::vector<uint8_t> captured = capture.store.disk;
  capture.store.failAfter = false; CHECK(capture.e.retrySave() == CommandResult::Applied);
  CHECK(capture.store.disk == captured && capture.e.view().recentSize == 1);
  Engine restart(capture.store); CHECK(restart.begin() == CommandResult::Applied);
  CHECK(restart.view().recentSize == 1 && restart.view().result == Result::Captured);
  CHECK(restart.pressThrow(AttemptKey(restart.view().encounter.id, 1), 0, reading(restart, 0, 0, false)) == CommandResult::Duplicate);
  World full; full.walkToEncounter(); CHECK(full.e.startBattle(full.now) == CommandResult::Applied);
  StepEvent many[129]; for (size_t i = 0; i < 129; ++i) { many[i].batchId = full.e.view().totalSteps + 1 + i; many[i].clock = full.clock(); }
  CHECK(full.e.observeSteps(many, 129, 1) == CommandResult::BufferFull && full.e.stepsIncomplete());
  CHECK(full.e.retreat() == CommandResult::Applied);
  CHECK(full.e.view().totalSteps == full.e.view().explorationSpent + 128 && full.e.view().stepsIncomplete);
  for (LoadStatus status : {LoadStatus::Corrupt, LoadStatus::UnsupportedVersion, LoadStatus::IoError, LoadStatus::NeedsProvisioning}) {
    MemoryStore store; store.forced = status; Engine e(store);
    CHECK(e.begin() == CommandResult::StorageError && !e.ready() && store.commits == 0);
  }
  MemoryStore boot; boot.failBefore = true; Engine e(boot);
  CHECK(e.begin() == CommandResult::StorageError && !e.ready());
  boot.failBefore = false; CHECK(e.retrySave() == CommandResult::Applied && e.ready());
  std::puts("PASS failed writes, frozen retry, uncertain commit, replay queue, overflow and load errors");
}
static void probabilityBoundaries() {
  const uint8_t probabilities[2][3] = {{85,92,98},{70,80,90}};
  const Timing timings[] = {Timing::Normal, Timing::Nice, Timing::Perfect};
  for (unsigned rare = 0; rare < 2; ++rare) for (unsigned timing = 0; timing < 3; ++timing) {
    for (unsigned side = 0; side < 2; ++side) {
      World w;
      const uint8_t roll = uint8_t(probabilities[rare][timing] - 1 + side);
      uint32_t seed = 0;
      for (; seed < 1000000; ++seed) {
        Encounter e; e.id = 1; e.seed = seed; e.species = chooseEncounter(w.e.view(), Route::Forest, seed);
        if (catalogEntry(e.species)->rare == bool(rare) && throwRoll(e, 1, kRuleVersion) == roll) break;
      }
      CHECK(seed < 1000000); w.walkToEncounter(seed); w.win();
      CHECK(w.ball(1, timings[timing]) == CommandResult::Applied);
      const ThrowAttempt& a = w.e.view().encounter.attempts[0];
      CHECK(a.probability == probabilities[rare][timing] && a.roll == roll && a.captured == !side);
    }
  }
  std::puts("PASS all six probability thresholds at p-1 and p using raw press timestamps");
}
static void additionalRecovery() {
  World w; w.walkToEncounter(seedFor(w.e.view(), false, true)); w.win();
  CHECK(w.ball(1) == CommandResult::Applied);
  Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  const uint64_t id = reboot.view().encounter.id;
  CHECK(reboot.view().phase == Phase::ThrowReady && reboot.view().encounter.usedBalls == 1);
  CHECK(reboot.beginThrow(AttemptKey(id, 1), 10) == CommandResult::Duplicate);
  CHECK(reboot.beginThrow(AttemptKey(id, 2), 10) == CommandResult::Applied);
  CHECK(reboot.pressThrow(AttemptKey(id, 2), reboot.throwView().targetMs,
        reading(reboot, 0, reboot.throwView().targetMs, false)) == CommandResult::Applied);
  CHECK(reboot.view().recentSize == 1);
  World prize; prize.walkToEncounter(seedFor(prize.e.view(), true, true)); prize.win();
  prize.store.failBefore = true; CHECK(prize.ball(1) == CommandResult::StorageError);
  prize.store.failBefore = false; Engine powerLoss(prize.store); CHECK(powerLoss.begin() == CommandResult::Applied);
  CHECK(powerLoss.view().phase == Phase::ThrowReady && powerLoss.view().recentSize == 0);
  CHECK(powerLoss.beginThrow(AttemptKey(powerLoss.view().encounter.id, 1), 100) == CommandResult::Applied);
  CHECK(powerLoss.pressThrow(AttemptKey(powerLoss.view().encounter.id, 1), powerLoss.throwView().targetMs,
        reading(powerLoss, 0, 1600, false)) == CommandResult::Applied);
  CHECK(powerLoss.view().result == Result::Captured && powerLoss.view().recentSize == 1);
  World overflow; const uint16_t target = overflow.e.view().encounterTarget;
  overflow.steps(target - 1); overflow.store.failBefore = true;
  StepEvent event; event.batchId = target; event.clock = overflow.clock();
  CHECK(overflow.e.observeSteps(&event, 1, 1) == CommandResult::StorageError);
  StepEvent many[129]; for (unsigned i = 0; i < 129; ++i) { many[i].batchId = target + 1 + i; many[i].clock = overflow.clock(); }
  CHECK(overflow.e.observeSteps(many, 129, 1) == CommandResult::BufferFull);
  overflow.store.failBefore = false; overflow.store.failAt = overflow.store.commits + 2;
  CHECK(overflow.e.retrySave() == CommandResult::StorageError && overflow.e.stepsIncomplete());
  CHECK(overflow.e.view().totalSteps == target && overflow.e.bufferedSteps() == 0);
  CHECK(overflow.e.retrySave() == CommandResult::Applied);
  CHECK(overflow.e.view().totalSteps == target + 128 && overflow.e.stepsIncomplete());
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
    World w; w.walkToEncounter(); CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
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
  World w; w.walkToEncounter(seedFor(w.e.view(), true, true));
  const uint16_t nextTarget = w.e.view().encounterTarget;
  w.steps(nextTarget + 100, 7); w.win(); CHECK(w.ball(1) == CommandResult::Applied);
  w.store.failAfter = true; CHECK(w.e.acknowledgeResult(1, 77) == CommandResult::StorageError);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().encounter.id == 1);
  w.store.failAfter = false; Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::EncounterReady && reboot.view().encounter.id == 2 && reboot.view().explorationCredit == 100);
  CHECK(reboot.view().recentSize == 1);
  std::puts("PASS victory and result acknowledgement crash points preserve encounter and credit");
}
int main() {
  basicSteps(); battles(); throwsAndRecords(); failures(); dates(); clockTransactions();
  probabilityBoundaries(); additionalRecovery(); battleCommitRecovery();
  std::printf("adventure: %u checks passed; sizeof(Save)=%zu sizeof(Engine)=%zu\n", checks, sizeof(Save), sizeof(Engine));
}
