#pragma once
#include "harness.hpp"
#include "dragon_trial_catalog.hpp"

// ---------------------------------------------------------------------------
// Dragon cave trial domain cases. Qualification only reads the unified 251
// capture counts; every state change goes through the same Engine commands the
// Runtime uses.
// ---------------------------------------------------------------------------

static void dragonMilestones(Save& s) {
  static const uint64_t life[] = {10000, 50000, 100000};
  for (unsigned index = 0; index < 3; ++index) {
    s.lifetime[index].reached = s.totalSteps >= life[index];
    if (s.lifetime[index].reached) {
      s.lifetime[index].first.bootGeneration = s.bootGeneration;
      s.lifetime[index].first.totalSteps = life[index];
    }
  }
}
static Save dragonSave(uint64_t captures, uint16_t capturedSpecies = 0) {
  Save s;
  s.sequence = 3; s.bootGeneration = 1; s.starter = s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = captures * kEncounterSteps;
  s.nextEncounterId = captures + 1;
  if (captures) {
    const uint16_t species = capturedSpecies ? capturedSpecies : 1;
    setRegistered(s, species); setOwned(s, species);
    s.captureCounts[species - 1] = uint32_t(captures);
    Capture& card = s.firstCapture[species - 1];
    card.species = species; card.route = Route::Forest;
    card.at.bootGeneration = 1; card.at.totalSteps = 500;
  }
  dragonMilestones(s);
  CHECK(validate(s));
  return s;
}
struct DragonWorld {
  MemoryStore store; Engine e; uint32_t now = 1000;
  explicit DragonWorld(const Save& fixture) : e(store) {
    CHECK(store.commit(fixture) == CommitStatus::Committed);
    CHECK(e.begin() == CommandResult::Applied);
  }
  explicit DragonWorld(uint64_t captures = 6) : DragonWorld(dragonSave(captures)) {}
  ClockReading clock() {
    ClockReading r;
    r.trusted = false;
    r.bootGeneration = e.view().bootGeneration;
    r.calibrationEpoch = e.view().clock.calibrationEpoch;
    r.monotonicMs = now;
    return r;
  }
  void steps(unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
      now += 500;
      StepEvent event; event.batchId = e.view().lastStepBatch + 1;
      event.clock = clock(); event.partner = e.view().partner;
      CHECK(e.observeSteps(&event, 1, 9) == CommandResult::Applied);
    }
  }
  CommandResult ball(uint8_t index, Timing timing = Timing::Perfect) {
    AttemptKey key(e.view().encounter.id, index);
    CHECK(e.beginThrow(key, now) == CommandResult::Applied);
    now = e.throwView().targetMs + (timing == Timing::Perfect ? 0 : 150);
    return e.pressThrow(key, now, clock());
  }
};
static uint32_t dragonSeed(DragonWorld& w, uint16_t species, bool first, bool second) {
  for (uint32_t seed = 0; seed < 2000000; ++seed) {
    Encounter probe; probe.seed = seed; probe.id = w.e.view().nextEncounterId;
    probe.species = species; probe.ruleVersion = 2;
    const uint8_t p = captureProbability(catalogEntry(species)->rare, Timing::Perfect);
    if ((throwRoll(probe, 1) < p) == first && (throwRoll(probe, 2) < p) == second) return seed;
  }
  CHECK(false); return 0;
}
static Result dragonFinish(DragonWorld& w, bool exact = true, uint8_t missDodges = 0) {
  CHECK(w.e.view().phase == Phase::Battle);
  uint8_t missed = 0;
  for (unsigned guard = 0; w.e.view().phase == Phase::Battle && guard < 20; ++guard) {
    const BattleView b = w.e.battleView();
    w.now = b.targetMs;
    if (b.expected == Action::Dodge && missed < missDodges) {
      ++missed;
      w.now = b.targetMs + b.normalWindowMs + 1;
      CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
      continue;
    }
    CHECK(w.e.press(b.expected, exact ? b.targetMs : b.targetMs + 100) == CommandResult::Applied);
    if (w.e.view().phase == Phase::Battle) {
      w.now += b.normalWindowMs + 1;
      CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
    }
  }
  CHECK(w.e.view().phase != Phase::Battle);
  return w.e.view().result;
}
static Result dragonPlay(DragonWorld& w, bool exact = true, uint8_t missDodges = 0) {
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  return dragonFinish(w, exact, missDodges);
}
static Result dragonTimeout(DragonWorld& w) {
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  for (unsigned guard = 0; w.e.view().phase == Phase::Battle && guard < 20; ++guard) {
    w.now = w.e.battleView().targetMs + w.e.battleView().normalWindowMs + 1;
    CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
  }
  return w.e.view().result;
}
static void dragonClose(DragonWorld& w) {
  CHECK(w.e.view().phase == Phase::Resolved);
  CHECK(w.e.acknowledgeActivityResult(w.e.view().encounter.id) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::Walking && !w.e.view().dragonTrial.active);
}

static void dragonCatalog() {
  CHECK(dragongen::kTrialCount == 2 && dragongen::kStageCount == 5);
  CHECK(dragonTrialSpecies(1) == 147 && dragonTrialSpecies(2) == 246);
  CHECK(dragonTrialStageCount(1) == 2 && dragonTrialStageCount(2) == 3);
  CHECK(!dragonTrialExists(0) && !dragonTrialExists(3));
  CHECK(dragonTrialForSpecies(147) == 1 && dragonTrialForSpecies(246) == 2 &&
        dragonTrialForSpecies(1) == 0);
  const uint16_t hp[] = {3, 4, 3, 4, 5};
  const uint16_t beat[] = {1500, 1450, 1500, 1450, 1350};
  uint8_t index = 0;
  for (uint8_t trialId = 1; trialId <= 2; ++trialId) {
    for (uint8_t stage = 0; stage < dragonTrialStageCount(trialId); ++stage, ++index) {
      const DragonStageSpec spec = dragonStageAt(trialId, stage);
      CHECK(spec.enemyHp == hp[index] && spec.beatMs == beat[index]);
    }
  }
  CHECK(dragonStageIsFinal(1, 1) && !dragonStageIsFinal(1, 0));
  CHECK(dragonStageIsFinal(2, 2) && !dragonStageIsFinal(2, 1));
  CHECK(dragonBonusLegal(0, 0) && dragonBonusLegal(30, 10) && dragonBonusLegal(180, 60));
  CHECK(!dragonBonusLegal(30, 20) && !dragonBonusLegal(20, 10) &&
        !dragonBonusLegal(180, 70) && !dragonBonusLegal(181, 60));
  CHECK(dragonNormalWindow(30) == 210 && dragonPerfectWindow(10) == 80);
  CHECK(dragonNormalWindow(180) == 360 && dragonPerfectWindow(60) == 130);
  std::puts("PASS dragon catalog: two independent trials, five stages and upgrade ceilings");
}

static void dragonTicketGate() {
  for (uint64_t captures : {uint64_t(0), uint64_t(5), uint64_t(7), uint64_t(11)}) {
    DragonWorld w(captures);
    CHECK(dragonCaptureTotal(w.e.view()) == captures);
    CHECK(!dragonTicketOpen(w.e.view()));
    CHECK(dragonCapturesUntilOpen(w.e.view()) ==
          (captures % 6 ? uint8_t(6 - captures % 6) : 6));
    CHECK(w.e.beginDragonTrial(1, 1) == CommandResult::NotReady);
    CHECK(w.e.beginDragonTrial(2, 1) == CommandResult::NotReady);
    CHECK(w.e.view().phase == Phase::Walking);
  }
  for (uint64_t captures : {uint64_t(6), uint64_t(12), uint64_t(18)}) {
    DragonWorld w(captures);
    CHECK(dragonTicketOpen(w.e.view()) && dragonCapturesUntilOpen(w.e.view()) == 0);
    CHECK(w.e.beginDragonTrial(1, 5) == CommandResult::Applied);
    CHECK(w.e.view().dragonTrial.trialId == 1);
    CHECK(w.e.retreat() == CommandResult::Applied);
    dragonClose(w);
    CHECK(w.e.beginDragonTrial(2, 6) == CommandResult::Applied);
    CHECK(w.e.view().dragonTrial.trialId == 2);
    CHECK(w.e.retreat() == CommandResult::Applied);
    dragonClose(w);
  }
  // Gifts, ownership, registration, evolution progress and walking never open a
  // trial: only captureCounts sums to the ticket step.
  Save gifted = dragonSave(5);
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    setRegistered(gifted, id); setOwned(gifted, id); gifted.progress[id - 1].walked = 100;
  }
  gifted.totalSteps = gifted.undatedSteps = 100000;
  gifted.explorationCredit = 100000 - 5 * kEncounterSteps;
  dragonMilestones(gifted);
  CHECK(validate(gifted));
  DragonWorld w(gifted);
  CHECK(dragonCaptureTotal(w.e.view()) == 5);
  CHECK(w.e.beginDragonTrial(1, 1) == CommandResult::NotReady);
  Save rich = gifted;
  rich.captureCounts[0] = 6;
  rich.firstCapture[0].species = 1;
  rich.nextEncounterId = 7;
  rich.explorationCredit = 70000;
  rich.totalSteps = rich.undatedSteps = 100000;
  dragonMilestones(rich);
  CHECK(validate(rich));
  DragonWorld accepted(rich);
  CHECK(accepted.e.beginDragonTrial(2, 1) == CommandResult::Applied);
  std::puts("PASS dragon qualification is the capture-count multiple; gifts and steps do not count");
}

static void dragonStagesAndCarry() {
  DragonWorld w(6);
  const uint32_t seed = dragonSeed(w, 147, true, true);
  CHECK(w.e.beginDragonTrial(1, seed) == CommandResult::Applied);
  const uint64_t id = w.e.view().encounter.id;
  CHECK(w.e.view().encounter.origin == EncounterOrigin::DragonTrial);
  CHECK(w.e.view().encounter.species == 147 && w.e.view().encounter.opponent == 147);
  CHECK(w.e.view().activityEncounterCount == 1);
  CHECK(w.e.view().dragonTrial.active && w.e.view().dragonTrial.stageIndex == 0 &&
        w.e.view().dragonTrial.stageStartHp == 3 && w.e.view().dragonTrial.guard == 0);
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  BattleView b = w.e.battleView();
  CHECK(b.playerMaxHp == 3 && b.playerHp == 3 && b.enemyMaxHp == 3 && b.enemyHp == 3);
  CHECK(b.beatMs == 1500 && b.normalWindowMs == 180 && b.perfectWindowMs == 70 && b.guard == 0);
  w.now = b.targetMs;
  while (w.e.view().phase == Phase::Battle) {
    b = w.e.battleView();
    w.now = b.targetMs;
    CHECK(w.e.press(b.expected, w.now) == CommandResult::Applied);
    if (w.e.view().phase == Phase::Battle) {
      w.now += b.normalWindowMs + 1;
      CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
    }
  }
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::DragonUpgrade);
  CHECK(w.e.view().dragonTrial.stageIndex == 1 && w.e.view().dragonTrial.stageStartHp == 3);
  CHECK(w.e.view().encounter.id == id && w.e.view().activityEncounterCount == 1);
  const uint64_t commits = w.store.commits;
  CHECK(w.e.chooseDragonUpgrade(0) == CommandResult::InvalidInput);  // already full
  CHECK(w.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  CHECK(w.store.commits > commits);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().dragonTrial.normalBonus == 30 && w.e.view().dragonTrial.perfectBonus == 10);
  CHECK(w.e.chooseDragonUpgrade(1) == CommandResult::InvalidPhase);  // no repeat
  CHECK(w.e.view().dragonTrial.normalBonus == 30 && w.e.view().dragonTrial.perfectBonus == 10);
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  b = w.e.battleView();
  CHECK(b.enemyHp == 4 && b.beatMs == 1450 && b.normalWindowMs == 210 && b.perfectWindowMs == 80);
  CHECK(b.playerHp == 3);
  while (w.e.view().phase == Phase::Battle) {
    b = w.e.battleView();
    w.now = b.targetMs;
    CHECK(w.e.press(b.expected, w.now) == CommandResult::Applied);
    if (w.e.view().phase == Phase::Battle) {
      w.now += b.normalWindowMs + 1;
      CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
    }
  }
  CHECK(w.e.view().phase == Phase::ThrowReady && w.e.view().result == Result::Won);
  CHECK(w.e.view().encounter.id == id && w.e.view().encounter.usedBalls == 0);
  CHECK(w.e.view().dragonTrial.stageIndex == 1 && w.e.view().dragonTrial.guard == 0);
  CHECK(w.ball(1) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Captured && w.e.view().captureCounts[146] == 1);
  CHECK(dragonCaptureTotal(w.e.view()) == 7);
  dragonClose(w);
  // At seven captures the other trial is closed until the next multiple.
  CHECK(w.e.beginDragonTrial(2, 9) == CommandResult::NotReady);

  // Larvitar carries HP, heals, then buys the missed-dodge guard.
  DragonWorld l(6);
  CHECK(l.e.beginDragonTrial(2, 77) == CommandResult::Applied);
  // Normal hits stretch the stage so two missed dodges land before the win.
  CHECK(dragonPlay(l, false, 2) == Result::DragonUpgrade);
  CHECK(l.e.view().dragonTrial.stageIndex == 1 && l.e.view().dragonTrial.stageStartHp == 1);
  CHECK(l.e.chooseDragonUpgrade(0) == CommandResult::Applied);
  CHECK(l.e.view().dragonTrial.stageStartHp == 2);
  CHECK(l.e.startBattle(l.now) == CommandResult::Applied);
  CHECK(l.e.battleView().playerHp == 2 && l.e.battleView().enemyHp == 4);
  while (l.e.view().phase == Phase::Battle) {
    const BattleView v = l.e.battleView();
    l.now = v.targetMs;
    CHECK(l.e.press(v.expected, l.now) == CommandResult::Applied);
    if (l.e.view().phase == Phase::Battle) {
      l.now += v.normalWindowMs + 1;
      CHECK(l.e.tick(l.now, l.clock()) == CommandResult::Applied);
    }
  }
  CHECK(l.e.view().result == Result::DragonUpgrade);
  CHECK(l.e.view().dragonTrial.stageIndex == 2 && l.e.view().dragonTrial.stageStartHp == 2);
  CHECK(l.e.chooseDragonUpgrade(2) == CommandResult::Applied);
  CHECK(l.e.view().dragonTrial.guard == 1);
  CHECK(l.e.startBattle(l.now) == CommandResult::Applied);
  CHECK(l.e.battleView().enemyHp == 5 && l.e.battleView().beatMs == 1350 &&
        l.e.battleView().guard == 1 && l.e.battleView().playerHp == 2);
  // The guard absorbs the first missed dodge, so the carried HP survives.
  CHECK(dragonFinish(l, true, 1) == Result::Won);
  CHECK(l.e.view().phase == Phase::ThrowReady);
  std::puts("PASS dragon stages: per-stage HP/beat, carried HP, heal and guard, one activity ID");
}

static void dragonCoreCases() {
  dragonCatalog();
  dragonTicketGate();
  dragonStagesAndCarry();
}
