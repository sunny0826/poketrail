#include "support/adventure_test/harness.hpp"
#include "support/adventure_test/clock_cases.hpp"

static void basicSteps() {
  World w;
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
  CHECK(w.e.selectRoute(Route::Johto) == CommandResult::Applied);
  CHECK(w.e.view().encounter.species == target.species && w.e.view().encounter.route == target.route);
  StepEvent replay; replay.batchId = w.e.view().totalSteps; replay.clock = w.clock();
  CHECK(w.e.observeSteps(&replay, 1, 9) == CommandResult::Duplicate);
  CHECK(w.e.retreat() == CommandResult::Applied);
  const uint64_t spentBefore = w.e.view().explorationSpent;
  CHECK(w.e.acknowledgeResult(target.id, 123) == CommandResult::Applied);
  CHECK(w.e.view().explorationSpent == spentBefore + nextTarget);
  CHECK(w.e.view().explorationCredit == 11000 - nextTarget && w.e.view().encounter.id == 2 && w.e.view().encounter.route == Route::Johto);
  Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  StepEvent fresh; fresh.batchId = 1; fresh.clock = reading(reboot, 0, 10, false);
  const uint64_t total = w.e.view().totalSteps;
  CHECK(reboot.observeSteps(&fresh, 1, 9) == CommandResult::Applied && reboot.view().totalSteps == total + 1);
  CHECK(reboot.observeSteps(&replay, 1, 9) == CommandResult::InvalidInput);
  std::puts("PASS random encounter target, route lock, ownership, replay and boot generation");
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
  World unknown; unknown.walkToEncounter(seedFor(unknown.e.view(), true, true, 4)); unknown.win();
  CHECK(unknown.ball(1) == CommandResult::Applied);
  CHECK(unknown.e.view().captureCounts[3] == 1 && !unknown.e.view().firstCapture[3].at.timeKnown);
  CHECK(unknown.e.view().firstCapture[3].at.bootGeneration == unknown.e.view().bootGeneration);
  CHECK(unknown.e.view().firstCapture[3].at.monotonicMs == unknown.now);
  unknown.calibrate(); CHECK(!unknown.e.view().firstCapture[3].at.timeKnown);
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
  CHECK(w.e.selectRoute(Route::Hoenn) == CommandResult::SavePending);
  StepEvent next[3]; for (unsigned i = 0; i < 3; ++i) { next[i].batchId = target + 1 + i; next[i].clock = w.clock(); }
  CHECK(w.e.observeSteps(next, 3, 19) == CommandResult::SavePending);
  CHECK(w.e.observeSteps(next, 3, 19) == CommandResult::Duplicate);
  CHECK(w.e.retrySave() == CommandResult::StorageError && frozen == w.store.lastAttempt);
  w.store.failBefore = false; CHECK(w.e.retrySave() == CommandResult::Applied);
  CHECK(w.e.view().totalSteps == target + 3 && w.e.view().explorationCredit == 3);
  CHECK(w.e.checkpoint() == CommandResult::Applied);
  for (LoadStatus status : {LoadStatus::Corrupt, LoadStatus::UnsupportedVersion, LoadStatus::IoError, LoadStatus::NeedsProvisioning}) {
    MemoryStore store; store.forced = status; Engine e(store);
    CHECK(e.begin() == CommandResult::StorageError && !e.ready() && store.commits == 0);
  }
  std::puts("PASS failed encounter write keeps a frozen candidate, queues steps once and refuses broken storage");
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
        Encounter e; e.id = 1; e.seed = seed; e.species = chooseEncounter(w.e.view(), Route::Kanto, seed);
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
  World overflow; const uint16_t target = overflow.e.view().encounterTarget;
  overflow.steps(target - 1); overflow.store.failBefore = true;
  StepEvent event; event.batchId = target; event.clock = overflow.clock();
  CHECK(overflow.e.observeSteps(&event, 1, 1) == CommandResult::StorageError);
  StepEvent many[129]; for (unsigned i = 0; i < 129; ++i) { many[i].batchId = target + 1 + i; many[i].clock = overflow.clock(); }
  CHECK(overflow.e.observeSteps(many, 129, 1) == CommandResult::BufferFull);
  overflow.store.failBefore = false; overflow.store.failAt = overflow.store.commits + 2;
  CHECK(overflow.e.retrySave() == CommandResult::StorageError && overflow.e.stepsIncomplete());
  CHECK(overflow.e.view().totalSteps == target);
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
  std::puts("PASS queued-step overflow remains visible through a second save failure; milestones keep their first step");
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
static void invalidRoutes() {
  World w;
  const std::vector<uint8_t> before = w.store.disk;
  for (Route legacy : {Route::Forest, Route::Coast, Route::Cave, Route(8)}) {
    CHECK(w.e.selectRoute(legacy) == CommandResult::InvalidInput);
    CHECK(w.store.disk == before && w.e.view().route == Route::Kanto);
  }
  std::puts("PASS invalid route commands preserve the selected region and saved state");
}
int main() {
  invalidRoutes(); basicSteps(); throwsAndRecords(); failures(); dates(); clockTransactions();
  probabilityBoundaries(); additionalRecovery(); battleCommitRecovery();
  std::printf("adventure: %u checks passed\n", checks);
}
