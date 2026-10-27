#pragma once
#include "dragon_cases.hpp"

// Dragon retry, capture, replay, zero-ball, ledger and rejection cases. They
// reuse the fixtures and battle drivers from dragon_cases.hpp.
static void dragonBattleRetry() {
  DragonWorld w(6);
  const uint32_t seed = 111;
  CHECK(w.e.beginDragonTrial(2, seed) == CommandResult::Applied);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  CHECK(dragonTimeout(w) == Result::Lost);
  CHECK(w.e.view().dragonTrial.stageIndex == 1 &&
        w.e.view().dragonTrial.normalBonus == 30);
  CHECK(w.e.retryDragonTrial(id + 1, 1) == CommandResult::InvalidInput);
  CHECK(w.e.retryDragonTrial(id, 999) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().encounter.id == id && w.e.view().encounter.seed == seed);
  CHECK(w.e.view().activityEncounterCount == 1);
  CHECK(w.e.view().dragonTrial.stageIndex == 1 &&
        w.e.view().dragonTrial.stageStartHp == 3);
  // Restarting a live battle returns to the same stage with the saved snapshot.
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  Engine reboot(w.store);
  CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::EncounterReady &&
        reboot.view().encounter.id == id && reboot.view().dragonTrial.stageIndex == 1);
  CHECK(reboot.startBattle(w.now) == CommandResult::Applied);
  CHECK(reboot.battleView().enemyHp == 4 && reboot.battleView().normalWindowMs == 210);
  // An uncertain publish keeps the same candidate and advances exactly once.
  w.store.failAfter = true;
  bool failed = false;
  while (reboot.view().phase == Phase::Battle) {
    const BattleView b = reboot.battleView();
    w.now = b.targetMs;
    const CommandResult result = reboot.press(b.expected, w.now);
    if (result == CommandResult::StorageError) { failed = true; break; }
    CHECK(result == CommandResult::Applied);
    if (reboot.view().phase == Phase::Battle) {
      w.now += b.normalWindowMs + 1;
      reboot.tick(w.now, w.clock());
    }
  }
  CHECK(failed && reboot.savePending());
  CHECK(reboot.view().phase == Phase::Battle && reboot.view().dragonTrial.stageIndex == 1);
  w.store.failAfter = false;
  CHECK(reboot.retrySave() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::Resolved &&
        reboot.view().result == Result::DragonUpgrade);
  CHECK(reboot.view().dragonTrial.stageIndex == 2);
  std::puts("PASS dragon defeat restores the current stage with the same ID and seed");
}

static void dragonCheckpointResume() {
  // The Runtime defers checkpoints while a battle runs; pausing first makes the
  // durable snapshot a Phase::Battle save, which boot turns into EncounterReady
  // for the same stage instead of the EncounterReady write used by startBattle.
  DragonWorld w(6);
  CHECK(w.e.beginDragonTrial(2, 77) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  const uint32_t normalBefore = w.e.view().dragonTrial.normalBonus;
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  CHECK(w.e.requestPause() == CommandResult::Applied);
  // The pause request lands on the next cue boundary; the runtime only
  // checkpoints once the battle is actually paused.
  w.now = w.e.battleView().targetMs + w.e.battleView().normalWindowMs + 1;
  CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
  CHECK(w.e.battleView().paused);
  CHECK(w.e.checkpoint() == CommandResult::Applied);
  Save durable;
  CHECK(codec::decode(w.store.disk.data(), w.store.disk.size(), durable) == codec::DecodeStatus::Ok);
  CHECK(durable.phase == Phase::Battle && durable.result == Result::None);
  CHECK(durable.dragonTrial.stageIndex == 1 && durable.dragonTrial.normalBonus == normalBefore &&
        durable.dragonTrial.perfectBonus == 10);
  Engine reboot(w.store);
  CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::EncounterReady && reboot.view().result == Result::None);
  CHECK(reboot.view().dragonTrial.stageIndex == 1 &&
        reboot.view().dragonTrial.normalBonus == normalBefore &&
        reboot.view().encounter.id == durable.encounter.id);
  CHECK(reboot.startBattle(w.now) == CommandResult::Applied);
  CHECK(reboot.battleView().enemyHp == 4 && reboot.battleView().beatMs == 1450 &&
        reboot.battleView().normalWindowMs == 210 && reboot.battleView().perfectWindowMs == 80);
  std::puts("PASS dragon checkpoint writes Phase::Battle and boots back to the same stage");
}

static void dragonCaptureRetry() {
  DragonWorld w(6);
  const uint32_t seed = dragonSeed(w, 147, false, false);
  CHECK(w.e.beginDragonTrial(1, seed) == CommandResult::Applied);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(2) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::Won);
  CHECK(w.ball(1) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Won);
  CHECK(w.ball(2) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Escaped && w.e.view().encounter.usedBalls == 2);
  const DragonTrialState snapshot = w.e.view().dragonTrial;
  const uint64_t spent = w.e.view().ballsSpent;
  CHECK(w.e.retryDragonTrial(id, 1234) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().encounter.id == id + 1 && w.e.view().encounter.seed == 1234);
  CHECK(w.e.view().encounter.usedBalls == 0 && !w.e.view().encounter.attempts[0].used);
  CHECK(w.e.view().activityEncounterCount == 2);
  CHECK(w.e.view().dragonTrial.stageIndex == snapshot.stageIndex &&
        w.e.view().dragonTrial.guard == snapshot.guard &&
        w.e.view().dragonTrial.stageStartHp == snapshot.stageStartHp);
  CHECK(w.e.view().ballsSpent == spent && w.e.view().captureCounts[146] == 0);
  // The retry starts directly at the final stage, never replaying stage one.
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  CHECK(w.e.battleView().enemyHp == 4 && w.e.view().dragonTrial.stageIndex == 1);
  CHECK(w.e.retryDragonTrial(w.e.view().encounter.id, 1) == CommandResult::InvalidPhase);
  std::puts("PASS two failed throws retry only the final stage with a new ID and seed");
}

static void dragonReplayNoReward() {
  Save fixture = dragonSave(6, 147);
  CHECK(fixture.captureCounts[146] == 6);
  DragonWorld w(fixture);
  CHECK(w.e.beginDragonTrial(1, 3) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  const uint32_t counts = w.e.view().captureCounts[146];
  const uint64_t spent = w.e.view().ballsSpent;
  const Stamp first = w.e.view().firstCapture[146].at;
  CHECK(dragonPlay(w) == Result::DragonCleared);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::DragonCleared);
  CHECK(w.e.view().encounter.usedBalls == 0);
  CHECK(w.e.view().captureCounts[146] == counts && w.e.view().ballsSpent == spent);
  CHECK(w.e.view().firstCapture[146].at.totalSteps == first.totalSteps);
  CHECK(w.e.view().recentSize == 0);
  CHECK(w.e.beginThrow(AttemptKey(w.e.view().encounter.id, 1), w.now) == CommandResult::InvalidPhase);
  dragonClose(w);
  std::puts("PASS replaying a captured dragon skips the throw and every reward");
}

static void dragonZeroBalls() {
  Save fixture = dragonSave(6);
  fixture.rewardBaseSteps = fixture.totalSteps;
  DragonWorld w(fixture);
  CHECK(w.e.beginDragonTrial(1, 21) == CommandResult::Applied);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(2) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::Won);
  CHECK(w.e.view().phase == Phase::ThrowReady && availableBalls(w.e.view()) == 0);
  CHECK(w.e.beginThrow(AttemptKey(id, 1), w.now) == CommandResult::NoBalls);
  Engine reboot(w.store);
  CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::ThrowReady && reboot.view().encounter.id == id);
  CHECK(reboot.view().result == Result::Won && reboot.view().dragonTrial.active);
  std::puts("PASS zero balls keeps the won final stage across a restart");
}

static void dragonMixedLedgerAndIndependence() {
  Save fixture = dragonSave(6);
  fixture.nextEncounterId = 8; fixture.activityEncounterCount = 1;
  fixture.ruins.clueMask = kRuinsAllClues; fixture.ruins.unlocked = true;
  fixture.ruins.lastTarget = 251;
  CHECK(validate(fixture));
  DragonWorld w(fixture);
  CHECK(w.e.beginDragonTrial(1, 8) == CommandResult::Applied);
  const RuinsState ruins = w.e.view().ruins;
  // Activity play banks walking credit but never charges the 5000-step fee.
  w.steps(6000);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().explorationCredit == 6000);
  CHECK(w.e.view().encounter.origin == EncounterOrigin::DragonTrial);
  CHECK(w.e.view().ruins.unlocked == ruins.unlocked &&
        w.e.view().ruins.lastTarget == ruins.lastTarget);
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::Won);
  CHECK(w.e.giveUpCapture(w.e.view().encounter.id) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Released && w.e.view().ballsSpent == 0);
  CHECK(w.e.acknowledgeActivityResult(w.e.view().encounter.id) == CommandResult::Applied);
  CHECK(w.e.view().explorationCredit == 6000 && !w.e.view().dragonTrial.active);
  // The ruins stay independent and usable after a dragon run.
  CHECK(w.e.beginRuinsBoss(9) == CommandResult::Applied);
  CHECK(w.e.view().encounter.origin == EncounterOrigin::Ruins);
  CHECK(w.e.retreat() == CommandResult::Applied);
  CHECK(w.e.acknowledgeActivityResult(w.e.view().encounter.id) == CommandResult::Applied);
  CHECK(w.e.view().ruins.clueMask == ruins.clueMask && w.e.view().ruins.unlocked);
  // The next ordinary encounter redeems the banked credit exactly once.
  w.steps(1);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().encounter.origin == EncounterOrigin::Ordinary);
  CHECK(w.e.view().explorationCredit == 1001);
  CHECK(w.e.view().activityEncounterCount == 3);
  std::puts("PASS dragon play banks steps, never drives ruins and resumes ordinary credit");
}

static void dragonValidation() {
  // A real stage-0 win plus the window choice supplies the legal base; no
  // hand-built snapshot is treated as a normal save.
  DragonWorld w(6);
  CHECK(w.e.beginDragonTrial(2, 77) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  const Save base = w.e.view();
  CHECK(validate(base));
  std::vector<uint8_t> out(codec::maxEncodedSize); size_t n = 0;
  CHECK(codec::encode(base, out.data(), out.size(), n));
  {
    Save back;
    CHECK(codec::decode(out.data(), n, back) == codec::DecodeStatus::Ok);
    CHECK(back.dragonTrial.active && back.dragonTrial.normalBonus == 30 &&
          back.dragonTrial.perfectBonus == 10 && back.phase == Phase::EncounterReady);
  }
  auto reject = [](const Save& s) {
    CHECK(!validate(s));
    std::vector<uint8_t> data(codec::maxEncodedSize); size_t written = 99;
    CHECK(!codec::encode(s, data.data(), data.size(), written) && written == 0);
  };
  Save bad = base; bad.encounter.origin = EncounterOrigin::Ruins; reject(bad);
  bad = base; bad.encounter.origin = EncounterOrigin::Ordinary; reject(bad);
  bad = base; bad.dragonTrial.active = false; reject(bad);
  bad = base; bad.dragonTrial.trialId = 0; reject(bad);
  bad = base; bad.dragonTrial.trialId = 3; reject(bad);
  bad = base; bad.dragonTrial.stageIndex = 3; reject(bad);
  bad = base; bad.dragonTrial.stageStartHp = 0; reject(bad);
  bad = base; bad.dragonTrial.stageStartHp = 4; reject(bad);
  bad = base; bad.dragonTrial.normalBonus = 20; reject(bad);
  bad = base; bad.dragonTrial.perfectBonus = 30; reject(bad);
  bad = base; bad.dragonTrial.normalBonus = 181; bad.dragonTrial.perfectBonus = 10; reject(bad);
  bad = base; bad.dragonTrial.normalBonus = 180; bad.dragonTrial.perfectBonus = 70; reject(bad);
  bad = base; bad.dragonTrial.guard = 2; reject(bad);
  bad = base; bad.encounter.species = bad.encounter.opponent = 147; reject(bad);
  bad = base; bad.encounter.opponent = 247; reject(bad);
  bad = base; bad.schema = 2; reject(bad);
  // The same window choice cannot also wait for the first upgrade.
  bad = base; bad.phase = Phase::Resolved; bad.result = Result::DragonUpgrade; reject(bad);
  bad = base; bad.phase = Phase::Resolved; bad.result = Result::DragonUpgrade;
  bad.dragonTrial.stageIndex = 0; reject(bad);
  bad = base; bad.phase = Phase::Resolved; bad.result = Result::DragonCleared; reject(bad);
  // A finished stage that reopens the throw for an already captured target.
  Save captured = base;
  captured.phase = Phase::ThrowReady; captured.result = Result::Won;
  captured.dragonTrial.stageIndex = 2;
  captured.captureCounts[245] = 1;
  captured.firstCapture[245].species = 246;
  captured.firstCapture[245].at.bootGeneration = 1;
  captured.firstCapture[245].at.totalSteps = 500;
  reject(captured);
  std::puts("PASS dragon stage, bonus, guard, origin, phase and result rejections");
}

static void dragonWriteBeforeRetry() {
  // A write-before failure on the stage-winning hit keeps the battle state and
  // publishes the next stage exactly once when the same candidate is retried.
  DragonWorld stage(6);
  CHECK(stage.e.beginDragonTrial(2, 5) == CommandResult::Applied);
  CHECK(stage.e.startBattle(stage.now) == CommandResult::Applied);
  bool failed = false;
  for (unsigned guard = 0; stage.e.view().phase == Phase::Battle && guard < 20; ++guard) {
    const BattleView b = stage.e.battleView();
    stage.now = b.targetMs;
    const bool lastHit = b.expected == Action::Attack && b.enemyHp <= 2;
    if (lastHit) stage.store.failBefore = true;
    const CommandResult result = stage.e.press(b.expected, stage.now);
    if (lastHit) {
      CHECK(result == CommandResult::StorageError && stage.e.savePending());
      failed = true; break;
    }
    CHECK(result == CommandResult::Applied);
    if (stage.e.view().phase == Phase::Battle) {
      stage.now += b.normalWindowMs + 1;
      CHECK(stage.e.tick(stage.now, stage.clock()) == CommandResult::Applied);
    }
  }
  CHECK(failed && stage.e.view().phase == Phase::Battle &&
        stage.e.view().dragonTrial.stageIndex == 0);
  stage.store.failBefore = false;
  CHECK(stage.e.retrySave() == CommandResult::Applied);
  CHECK(stage.e.view().phase == Phase::Resolved &&
        stage.e.view().result == Result::DragonUpgrade &&
        stage.e.view().dragonTrial.stageIndex == 1);
  CHECK(stage.e.retrySave() == CommandResult::Duplicate);

  // The same holds for the final throw: a failed publish cannot spend a second
  // ball, reroll or award the capture twice.
  DragonWorld w(6);
  const uint32_t seed = dragonSeed(w, 147, true, true);
  CHECK(w.e.beginDragonTrial(1, seed) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::DragonUpgrade);
  CHECK(w.e.chooseDragonUpgrade(2) == CommandResult::Applied);
  CHECK(dragonPlay(w) == Result::Won);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(w.e.beginThrow(AttemptKey(id, 1), w.now) == CommandResult::Applied);
  w.now = w.e.throwView().targetMs;
  w.store.failBefore = true;
  CHECK(w.e.pressThrow(AttemptKey(id, 1), w.now, w.clock()) == CommandResult::StorageError);
  CHECK(w.e.savePending() && w.e.view().phase == Phase::ThrowReady);
  CHECK(w.e.view().encounter.usedBalls == 0 && w.e.view().ballsSpent == 0 &&
        w.e.view().captureCounts[146] == 0);
  w.store.failBefore = false;
  CHECK(w.e.retrySave() == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Captured && w.e.view().captureCounts[146] == 1);
  CHECK(w.e.view().ballsSpent == 1 && w.e.view().encounter.usedBalls == 1);
  CHECK(w.e.retrySave() == CommandResult::Duplicate);
  std::puts("PASS write-before failures retry one dragon candidate without double rewards");
}

static void dragonRetryCases() {
  dragonBattleRetry();
  dragonCheckpointResume();
  dragonCaptureRetry();
  dragonReplayNoReward();
  dragonZeroBalls();
  dragonMixedLedgerAndIndependence();
  dragonValidation();
  dragonWriteBeforeRetry();
}
