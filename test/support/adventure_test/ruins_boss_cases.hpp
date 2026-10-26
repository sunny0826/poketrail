#pragma once
#include "ruins_cases.hpp"

// Boss, retry, save-failure, zero-ball and mixed-ledger cases. They reuse the
// fixtures and battle drivers from ruins_cases.hpp.
static void ruinsBossCaptureAndReplay() {
  RuinsWorld w(ruinsFundedSave());
  CHECK(w.e.selectRuinsTarget(144) == CommandResult::Applied);
  const uint32_t seed = ruinsSeed(w, 144, true, true);
  CHECK(w.e.beginRuinsBoss(seed) == CommandResult::Applied);
  CHECK(ruinsPlay(w) == Result::Won);
  CHECK(w.e.view().phase == Phase::ThrowReady && w.e.view().result == Result::Won);
  CHECK(w.e.view().encounter.usedBalls == 0);
  const uint64_t bossId = w.e.view().encounter.id;
  CHECK(w.ball(1) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::Captured);
  CHECK(w.e.view().captureCounts[143] == 1 && w.e.view().firstCapture[143].species == 144);
  CHECK(w.e.view().ballsSpent == 1 && w.e.view().encounter.usedBalls == 1);
  CHECK(w.e.acknowledgeResult(bossId, 1) == CommandResult::InvalidPhase);
  ruinsClose(w);
  const uint32_t count = w.e.view().captureCounts[143];
  const uint64_t spent = w.e.view().ballsSpent;
  const Stamp first = w.e.view().firstCapture[143].at;
  // Replaying a captured target skips the throw and every reward.
  CHECK(w.e.beginRuinsBoss(88) == CommandResult::Applied);
  CHECK(ruinsPlay(w) == Result::RuinsCleared);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::RuinsCleared);
  CHECK(w.e.view().encounter.usedBalls == 0);
  CHECK(w.e.view().captureCounts[143] == count && w.e.view().ballsSpent == spent);
  CHECK(w.e.view().firstCapture[143].at.totalSteps == first.totalSteps);
  CHECK(w.e.view().recentSize == 1);
  ruinsClose(w);
  std::puts("PASS boss capture awards once and a replay win skips throw and rewards");
}

static void ruinsBossEscapeRetry() {
  RuinsWorld w(ruinsFundedSave());
  CHECK(w.e.selectRuinsTarget(145) == CommandResult::Applied);
  const uint32_t seed = ruinsSeed(w, 145, false, false);
  CHECK(w.e.beginRuinsBoss(seed) == CommandResult::Applied);
  const uint64_t id = w.e.view().encounter.id;
  const uint32_t battleSeed = w.e.view().encounter.seed;
  CHECK(ruinsPlay(w) == Result::Won);
  CHECK(w.ball(1) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::ThrowReady && w.e.view().result == Result::Won);
  CHECK(w.e.view().encounter.usedBalls == 1 && w.e.view().ballsSpent == 1);
  CHECK(w.ball(2) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::Escaped);
  CHECK(w.e.view().ballsSpent == 2 && w.e.view().encounter.usedBalls == 2);
  CHECK(w.e.retryRuinsActivity(id + 1, 999) == CommandResult::InvalidInput);
  CHECK(w.e.retryRuinsActivity(id, battleSeed) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().encounter.id == id + 1 && w.e.view().encounter.id != id);
  CHECK(w.e.view().encounter.seed == battleSeed);
  CHECK(w.e.view().activityEncounterCount == 3);
  CHECK(w.e.view().ruins.unlocked && w.e.view().ruins.lastTarget == 145);
  CHECK(w.e.view().ballsSpent == 2 && !w.e.view().captureCounts[144]);
  CHECK(w.e.retryRuinsActivity(w.e.view().encounter.id, 1) == CommandResult::InvalidPhase);
  // Restart keeps the retry encounter and the clue/unlock progress.
  Engine reboot(w.store);
  CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::EncounterReady && reboot.view().encounter.origin == EncounterOrigin::Ruins);
  CHECK(reboot.view().ruins.unlocked && reboot.view().ruins.lastTarget == 145);
  std::puts("PASS two failed throws retry only the current boss with a new ID and seed");
}

static void ruinsBossFailureAndSaveRetry() {
  RuinsWorld w(ruinsFundedSave());
  CHECK(w.e.selectRuinsTarget(146) == CommandResult::Applied);
  CHECK(w.e.beginRuinsBoss(7) == CommandResult::Applied);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(ruinsTimeout(w) == Result::Lost);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::Lost);
  CHECK(w.e.view().ruins.unlocked && w.e.view().ruins.lastTarget == 146);
  CHECK(w.e.view().ballsSpent == 0);
  CHECK(w.e.retryRuinsActivity(id, 31) == CommandResult::Applied);
  CHECK(w.e.view().encounter.id == id + 1 && w.e.view().encounter.seed == 31);
  CHECK(w.e.view().activityEncounterCount == 3);
  // A failed publish keeps the same candidate and only retries the commit.
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  for (unsigned i = 0; w.e.view().phase == Phase::Battle && i < 8; ++i) {
    const BattleView b = w.e.battleView();
    w.now = b.targetMs;
    const bool lastHit = b.expected == Action::Attack && b.enemyHp <= 2;
    if (lastHit) w.store.failAfter = true;
    const CommandResult result = w.e.press(b.expected, w.now);
    if (lastHit) {
      CHECK(result == CommandResult::StorageError && w.e.savePending());
      CHECK(w.e.view().phase == Phase::Battle);
      break;
    }
    CHECK(result == CommandResult::Applied);
    w.now += kNormalWindowMs + 1;
    CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
  }
  const uint64_t pendingId = w.e.view().encounter.id;
  w.store.failAfter = false;
  CHECK(w.e.retrySave() == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::ThrowReady && w.e.view().encounter.id == pendingId);
  CHECK(w.e.view().result == Result::Won && w.e.view().ballsSpent == 0);
  CHECK(w.e.retrySave() == CommandResult::Duplicate);
  std::puts("PASS boss defeat and uncertain saves keep the stage and retry one candidate");
}

static void ruinsGiveUpRetry() {
  RuinsWorld w(ruinsFundedSave());
  CHECK(w.e.selectRuinsTarget(249) == CommandResult::Applied);
  CHECK(w.e.beginRuinsBoss(5) == CommandResult::Applied);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(ruinsPlay(w) == Result::Won);
  CHECK(w.e.giveUpCapture(id) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Released && w.e.view().ballsSpent == 0);
  // Giving up after a win is still only the current boss stage.
  CHECK(w.e.retryRuinsActivity(id, 6) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().encounter.id == id + 1 && w.e.view().encounter.seed == 6);
  CHECK(w.e.view().ruins.unlocked && w.e.view().ruins.lastTarget == 249);
  CHECK(w.e.view().activityEncounterCount == 3 && w.e.view().ballsSpent == 0);
  std::puts("PASS giving up after a win retries only the current boss");
}

static Save ruinsNoBallSave() {
  Save s = ruinsUnlockedSave();
  s.totalSteps = s.undatedSteps = 5000;
  s.explorationCredit = 5000;
  s.rewardBaseSteps = 5000;
  return s;
}
static void ruinsZeroBalls() {
  MemoryStore store;
  CHECK(store.commit(ruinsNoBallSave()) == CommitStatus::Committed);
  Engine e(store);
  CHECK(e.begin() == CommandResult::Applied);
  CHECK(e.selectRuinsTarget(150) == CommandResult::Applied);
  CHECK(e.beginRuinsBoss(3) == CommandResult::Applied);
  CHECK(e.startBattle(1000) == CommandResult::Applied);
  while (e.view().phase == Phase::Battle) {
    const uint32_t target = e.battleView().targetMs;
    CHECK(e.press(e.battleView().expected, target) == CommandResult::Applied);
    if (e.view().phase == Phase::Battle) {
      ClockReading r; r.bootGeneration = e.view().bootGeneration;
      CHECK(e.tick(target + 181, r) == CommandResult::Applied);
    }
  }
  CHECK(e.view().phase == Phase::ThrowReady && availableBalls(e.view()) == 0);
  const uint64_t id = e.view().encounter.id;
  CHECK(e.beginThrow(AttemptKey(id, 1), 5000) == CommandResult::NoBalls);
  Engine reboot(store);
  CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::ThrowReady && reboot.view().encounter.id == id);
  CHECK(availableBalls(reboot.view()) == 0 && reboot.view().result == Result::Won);
  std::puts("PASS zero balls keeps the won boss encounter across a restart");
}

static void ruinsMixedLedger() {
  RuinsWorld w;
  CHECK(w.e.selectRuinsTarget(249) == CommandResult::Applied);
  CHECK(w.e.beginRuinsBoss(4) == CommandResult::Applied);
  // Walking during an activity accumulates credit but never creates an ordinary
  // encounter and never charges the 5000-step fee.
  w.steps(6000);
  CHECK(w.e.view().phase == Phase::EncounterReady && w.e.view().explorationCredit == 6000);
  CHECK(w.e.view().encounter.origin == EncounterOrigin::Ruins);
  CHECK(ruinsPlay(w) == Result::Won);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(w.e.giveUpCapture(id) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Released && w.e.view().ballsSpent == 0);
  CHECK(w.e.acknowledgeActivityResult(id) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::Walking && w.e.view().encounter.id == 0);
  CHECK(w.e.view().explorationCredit == 6000);
  // The next step redeems the banked credit once and only once.
  w.steps(1);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().explorationCredit == 1001);
  CHECK(w.e.view().encounter.origin == EncounterOrigin::Ordinary);
  CHECK(w.e.view().encounter.species != 0 && w.e.view().activityEncounterCount == 2);
  CHECK(w.e.view().encounter.id == 3);
  std::puts("PASS activity play banks steps and resumes ordinary encounters with one 5000 fee");
}

static void ruinsPoolExhausted() {
  Save s = ruinsLockedSave();
  uint16_t count = 0;
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    if (!ordinaryEncounter(id)) continue;
    ++count;
    setRegistered(s, id); s.captureCounts[id - 1] = 1;
    Capture& card = s.firstCapture[id - 1];
    card.species = id; card.route = Route::Forest;
    card.at.bootGeneration = 1; card.at.totalSteps = 500;
  }
  s.nextEncounterId = count + 1;
  s.totalSteps = s.undatedSteps = uint64_t(count) * 5000;
  const uint64_t lifetime[] = {10000, 50000, 100000};
  for (unsigned i = 0; i < 3; ++i) {
    s.lifetime[i].reached = true;
    s.lifetime[i].first.bootGeneration = 1;
    s.lifetime[i].first.totalSteps = lifetime[i];
  }
  CHECK(validate(s));
  MemoryStore store;
  CHECK(store.commit(s) == CommitStatus::Committed);
  Engine e(store);
  CHECK(e.begin() == CommandResult::Applied);
  CHECK(remainingEncounters(e.view(), Route::Forest) == 0);
  CHECK(remainingEncounters(e.view(), Route::Coast) == 0);
  CHECK(remainingEncounters(e.view(), Route::Cave) == 0);
  for (uint8_t index = 0; index < kRuinsClueTotal; ++index) {
    CHECK(e.beginRuinsClue(index, 60 + index) == CommandResult::Applied);
    CHECK(e.startBattle(1000) == CommandResult::Applied);
    while (e.view().phase == Phase::Battle) {
      const uint32_t target = e.battleView().targetMs;
      CHECK(e.press(e.battleView().expected, target) == CommandResult::Applied);
      if (e.view().phase == Phase::Battle) {
        ClockReading r; r.bootGeneration = e.view().bootGeneration;
        CHECK(e.tick(target + 181, r) == CommandResult::Applied);
      }
    }
    CHECK(e.view().result == Result::RuinsClue);
    CHECK(e.acknowledgeActivityResult(e.view().encounter.id) == CommandResult::Applied);
  }
  uint8_t correct[3] = {1, 0, 2};
  CHECK(e.solveRuinsOrder(correct).status == CommandResult::Applied);
  CHECK(e.view().ruins.unlocked);
  std::puts("PASS an empty ordinary pool still completes all clues and the unlock");
}

static void ruinsBossCases() {
  ruinsBossCaptureAndReplay();
  ruinsBossEscapeRetry();
  ruinsBossFailureAndSaveRetry();
  ruinsGiveUpRetry();
  ruinsZeroBalls();
  ruinsMixedLedger();
  ruinsPoolExhausted();
}
