#pragma once
#include "harness.hpp"
#include "ruins_catalog.hpp"

// ---------------------------------------------------------------------------
// Legendary ruins domain cases. The Engine owns every state change; these tests
// drive the same public commands the Runtime uses.
// ---------------------------------------------------------------------------

static void ruinsCatalogTable() {
  const uint16_t opponents[] = {10, 54, 74};
  for (uint8_t index = 0; index < kRuinsClueTotal; ++index) {
    const RuinsClueSpec clue = ruinsClueAt(index);
    CHECK(clue.opponent == opponents[index]);
    CHECK(uint8_t(clue.route) == index);
    CHECK(clue.battle.playerHp == 3 && clue.battle.enemyHp == 3 && clue.battle.cueLimit == 12);
    CHECK(clue.battle.beatMs == 1500 && clue.battle.normalWindowMs == 180 &&
          clue.battle.perfectWindowMs == 70);
    CHECK(clue.battle.telegraph == TelegraphPattern::Alternate);
    CHECK(clue.battle.missedDodgeGuard == 0);
  }
  CHECK(ruinsClueAt(0).requiredPerfect == 0 && !ruinsClueAt(0).requireFullHp);
  CHECK(ruinsClueAt(1).requiredPerfect == 2 && !ruinsClueAt(1).requireFullHp);
  CHECK(ruinsClueAt(2).requiredPerfect == 0 && ruinsClueAt(2).requireFullHp);
  CHECK(ruinsOrderAt(0) == Route::Coast && ruinsOrderAt(1) == Route::Forest &&
        ruinsOrderAt(2) == Route::Cave);
  CHECK(ruinsClueOrderPosition(0) == 2 && ruinsClueOrderPosition(1) == 1 &&
        ruinsClueOrderPosition(2) == 3 && ruinsClueOrderPosition(3) == 0);
  CHECK(ruinsClueIndexForRoute(Route::Coast) == 1 && ruinsClueIndexForOpponent(74) == 2);
  CHECK(ruinsClueIndexForOpponent(25) == kRuinsNoIndex);

  const uint16_t ids[] = {144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251};
  const uint8_t hp[] = {4, 4, 4, 5, 4, 4, 4, 4, 5, 5, 4};
  const uint16_t beat[] = {1450, 1450, 1400, 1300, 1400, 1450, 1400, 1400, 1350, 1350, 1400};
  const TelegraphPattern pattern[] = {
    TelegraphPattern::Alternate, TelegraphPattern::AlternateDodgeFirst,
    TelegraphPattern::AttackOnly, TelegraphPattern::AttackOnly,
    TelegraphPattern::AlternateDodgeFirst, TelegraphPattern::AlternateDodgeFirst,
    TelegraphPattern::AttackOnly, TelegraphPattern::AlternateDodgeFirst,
    TelegraphPattern::Alternate, TelegraphPattern::AlternateDodgeFirst,
    TelegraphPattern::Alternate};
  for (uint8_t index = 0; index < kRuinsTargetTotal; ++index) {
    const RuinsTargetSpec target = ruinsTargetAt(index);
    CHECK(target.species == ids[index]);
    CHECK(target.battle.enemyHp == hp[index] && target.battle.enemyStartHp == hp[index]);
    CHECK(target.battle.beatMs == beat[index] && target.battle.telegraph == pattern[index]);
    CHECK(target.battle.playerHp == 3 && target.battle.playerStartHp == 3);
    CHECK(target.battle.cueLimit == 12 && target.battle.normalWindowMs == 180 &&
          target.battle.perfectWindowMs == 70 && target.battle.missedDodgeGuard == 0);
    CHECK(ruinsTargetIndex(ids[index]) == index && ruinsTargetExists(ids[index]));
  }
  CHECK(ruinsTargetIndex(1) == kRuinsNoIndex && !ruinsTargetExists(25));
  std::puts("PASS ruins catalog: three clues, Coast/Forest/Cave order and eleven boss specs");
}

static Save ruinsLockedSave() {
  Save s; s.sequence = 2; s.bootGeneration = 1; s.starter = s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  return s;
}
// One acknowledged activity encounter, all clues and the unlock. No ordinary ID
// was ever charged, so the books carry a single activity ID and zero credit.
static Save ruinsUnlockedSave() {
  Save s = ruinsLockedSave();
  s.nextEncounterId = 2; s.activityEncounterCount = 1;
  s.ruins.clueMask = kRuinsAllClues; s.ruins.unlocked = true;
  return s;
}
// Same state with five earned balls (and the matching banked credit).
static Save ruinsFundedSave() {
  Save s = ruinsUnlockedSave();
  s.totalSteps = s.undatedSteps = 5000;
  s.explorationCredit = 5000;
  return s;
}
struct RuinsWorld {
  MemoryStore store; Engine e; uint32_t now = 1000;
  explicit RuinsWorld(const Save& fixture) : e(store) {
    CHECK(store.commit(fixture) == CommitStatus::Committed);
    CHECK(e.begin() == CommandResult::Applied);
  }
  explicit RuinsWorld(bool unlocked = true)
      : RuinsWorld(unlocked ? ruinsUnlockedSave() : ruinsLockedSave()) {}
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
// Plays the current activity battle. `exact` presses on the beat (Perfect),
// otherwise 100ms late (Normal). `skipDodges` lets that many dodge cues time out.
static Result ruinsPlay(RuinsWorld& w, bool exact = true, uint8_t skipDodges = 0) {
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  uint8_t skipped = 0;
  for (unsigned guard = 0; w.e.view().phase == Phase::Battle && guard < 16; ++guard) {
    const BattleView b = w.e.battleView();
    w.now = b.targetMs;
    if (b.expected == Action::Dodge && skipped < skipDodges) {
      ++skipped;
      w.now = b.targetMs + kNormalWindowMs + 1;
      CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
      continue;
    }
    CHECK(w.e.press(b.expected, exact ? b.targetMs : b.targetMs + 100) == CommandResult::Applied);
    if (w.e.view().phase == Phase::Battle) {
      w.now += kNormalWindowMs + 1;
      CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
    }
  }
  CHECK(w.e.view().phase != Phase::Battle);
  return w.e.view().result;
}
static Result ruinsTimeout(RuinsWorld& w) {
  CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  for (unsigned guard = 0; w.e.view().phase == Phase::Battle && guard < 16; ++guard) {
    w.now = w.e.battleView().targetMs + kNormalWindowMs + 1;
    CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
  }
  return w.e.view().result;
}
static void ruinsClose(RuinsWorld& w) {
  CHECK(w.e.view().phase == Phase::Resolved);
  CHECK(w.e.acknowledgeActivityResult(w.e.view().encounter.id) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::Walking && w.e.view().encounter.id == 0);
}
static uint32_t ruinsSeed(RuinsWorld& w, uint16_t species, bool first, bool second) {
  for (uint32_t seed = 0; seed < 1000000; ++seed) {
    Encounter probe; probe.seed = seed; probe.id = w.e.view().nextEncounterId;
    probe.species = species;
    const uint8_t p = captureProbability(catalogEntry(species)->rare, Timing::Perfect);
    if ((throwRoll(probe, 1) < p) == first && (throwRoll(probe, 2) < p) == second) return seed;
  }
  CHECK(false); return 0;
}

static void ruinsClueFlow() {
  RuinsWorld w(false);
  const uint64_t steps = w.e.view().totalSteps;
  // Forest: a plain victory awards only the clue and never starts a throw.
  CHECK(w.e.beginRuinsClue(0, 5) == CommandResult::Applied);
  CHECK(w.e.view().phase == Phase::EncounterReady);
  CHECK(w.e.view().activityEncounterCount == 1 && w.e.view().encounter.id == 1);
  CHECK(w.e.view().encounter.origin == EncounterOrigin::Ruins &&
        w.e.view().encounter.species == 10 && w.e.view().encounter.opponent == 10);
  CHECK(w.e.view().encounter.route == Route::Forest);
  CHECK(w.e.view().explorationCredit == 0 && w.e.view().totalSteps == steps);
  CHECK(ruinsPlay(w) == Result::RuinsClue);
  CHECK(w.e.view().ruins.clueMask == 1 && !w.e.view().ruins.unlocked);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::RuinsClue);
  CHECK(w.e.view().encounter.usedBalls == 0 && w.e.view().ballsSpent == 0);
  CHECK(w.e.beginThrow(AttemptKey(w.e.view().encounter.id, 1), w.now) == CommandResult::InvalidPhase);
  // Ordinary acknowledgement may not close an activity result.
  CHECK(w.e.acknowledgeResult(w.e.view().encounter.id, 1) == CommandResult::InvalidPhase);
  ruinsClose(w);
  CHECK(w.e.beginRuinsClue(0, 6) == CommandResult::Duplicate);

  // Coast: winning with Normal hits only does not issue the clue.
  CHECK(w.e.beginRuinsClue(1, 7) == CommandResult::Applied);
  CHECK(ruinsPlay(w, false) == Result::Lost);
  CHECK(w.e.view().ruins.clueMask == 1);
  CHECK(w.e.view().phase == Phase::Resolved && w.e.view().encounter.id == 2);
  ruinsClose(w);
  CHECK(w.e.beginRuinsClue(1, 8) == CommandResult::Applied);
  CHECK(ruinsPlay(w) == Result::RuinsClue);
  CHECK(w.e.view().ruins.clueMask == 3);
  ruinsClose(w);

  // Cave: one missed dodge is damage, so the clue is withheld.
  CHECK(w.e.beginRuinsClue(2, 9) == CommandResult::Applied);
  CHECK(ruinsPlay(w, true, 1) == Result::Lost);
  CHECK(w.e.view().ruins.clueMask == 3);
  ruinsClose(w);
  CHECK(w.e.beginRuinsClue(2, 10) == CommandResult::Applied);
  CHECK(ruinsPlay(w) == Result::RuinsClue);
  CHECK(w.e.view().ruins.clueMask == kRuinsAllClues);
  CHECK(w.e.view().captureCounts[9] == 0 && w.e.view().captureCounts[53] == 0 &&
        w.e.view().captureCounts[73] == 0);
  ruinsClose(w);
  CHECK(w.e.view().phase == Phase::Walking && w.e.view().explorationCredit == 0);
  std::puts("PASS clue tasks award only their clue, never a throw, ball or capture");
}

static void ruinsPermutations() {
  const uint8_t orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  for (const auto& order : orders) {
    RuinsWorld w(false);
    for (uint8_t slot = 0; slot < kRuinsClueTotal; ++slot) {
      CHECK(w.e.beginRuinsClue(order[slot], 20 + slot) == CommandResult::Applied);
      CHECK(ruinsPlay(w) == Result::RuinsClue);
      ruinsClose(w);
    }
    CHECK(w.e.view().ruins.clueMask == kRuinsAllClues);
    uint8_t correct[3] = {1, 0, 2};
    CHECK(w.e.solveRuinsOrder(correct).status == CommandResult::Applied);
    CHECK(w.e.view().ruins.unlocked);
    // The unlock has to survive a real reboot from the stored bytes.
    Engine reboot(w.store);
    CHECK(reboot.begin() == CommandResult::Applied);
    CHECK(reboot.view().ruins.clueMask == kRuinsAllClues && reboot.view().ruins.unlocked);
  }
  std::puts("PASS all six clue orders reach the single permanent unlock");
}

static Save ruinsCapturedClueSave() {
  Save s = ruinsLockedSave();
  setRegistered(s, 10); setOwned(s, 10);
  s.captureCounts[9] = 1;
  Capture& card = s.firstCapture[9];
  card.species = 10; card.route = Route::Forest;
  card.at.bootGeneration = 1; card.at.totalSteps = 500;
  s.totalSteps = s.undatedSteps = 5000;
  s.nextEncounterId = 2; s.activityEncounterCount = 1; s.explorationCredit = 5000;
  s.encounter.id = 1; s.encounter.species = 10; s.encounter.opponent = 10;
  s.encounter.partner = 1; s.encounter.route = Route::Forest;
  s.encounter.origin = EncounterOrigin::Ruins;
  s.phase = Phase::EncounterReady;
  return s;
}
static void ruinsCapturedClueOpponent() {
  MemoryStore store;
  CHECK(store.commit(ruinsCapturedClueSave()) == CommitStatus::Committed);
  Engine e(store);
  CHECK(e.begin() == CommandResult::Applied);
  // Opponent 10 was captured in ordinary play earlier; the clue run must stay
  // open instead of being retired as a finished boss.
  CHECK(e.view().phase == Phase::EncounterReady && e.view().result == Result::None);
  CHECK(e.view().encounter.origin == EncounterOrigin::Ruins && e.view().encounter.species == 10);
  CHECK(e.startBattle(1000) == CommandResult::Applied);
  while (e.view().phase == Phase::Battle) {
    const uint32_t target = e.battleView().targetMs;
    CHECK(e.press(e.battleView().expected, target) == CommandResult::Applied);
    if (e.view().phase == Phase::Battle) {
      ClockReading r; r.bootGeneration = e.view().bootGeneration;
      CHECK(e.tick(target + 181, r) == CommandResult::Applied);
    }
  }
  CHECK(e.view().result == Result::RuinsClue && e.view().ruins.clueMask == 1);
  CHECK(e.view().captureCounts[9] == 1 && e.view().firstCapture[9].species == 10);
  std::puts("PASS a captured clue opponent cannot retire or reward the clue battle");
}

static void ruinsOrderPuzzle() {
  RuinsWorld w(false);
  uint8_t correct[3] = {1, 0, 2};
  CHECK(w.e.solveRuinsOrder(correct).status == CommandResult::InvalidPhase);
  CHECK(w.e.selectRuinsTarget(144) == CommandResult::InvalidPhase);
  CHECK(w.e.beginRuinsBoss(1) == CommandResult::InvalidPhase);
  for (uint8_t index = 0; index < kRuinsClueTotal; ++index) {
    CHECK(w.e.beginRuinsClue(index, 30 + index) == CommandResult::Applied);
    CHECK(ruinsPlay(w) == Result::RuinsClue);
    ruinsClose(w);
  }
  const Save before = w.e.view();
  const unsigned commits = w.store.commits;
  uint8_t wrong[3] = {0, 1, 2};
  RuinsOrderResult result = w.e.solveRuinsOrder(wrong);
  CHECK(result.status == CommandResult::InvalidInput && result.conflict == 0);
  uint8_t partial[3] = {1, 2, 0};
  result = w.e.solveRuinsOrder(partial);
  CHECK(result.status == CommandResult::InvalidInput && result.conflict == 1);
  CHECK(w.store.commits == commits);  // a wrong order writes nothing
  CHECK(w.e.view().sequence == before.sequence);
  CHECK(w.e.view().ruins.clueMask == before.ruins.clueMask && !w.e.view().ruins.unlocked);
  CHECK(availableBalls(w.e.view()) == availableBalls(before) &&
        w.e.view().totalSteps == before.totalSteps &&
        w.e.view().explorationCredit == before.explorationCredit &&
        w.e.view().nextEncounterId == before.nextEncounterId);
  uint8_t outOfRange[3] = {1, 0, 3};
  CHECK(w.e.solveRuinsOrder(outOfRange).status == CommandResult::InvalidInput);
  result = w.e.solveRuinsOrder(correct);
  CHECK(result.status == CommandResult::Applied && w.e.view().ruins.unlocked);
  CHECK(w.e.view().ruins.clueMask == kRuinsAllClues);
  result = w.e.solveRuinsOrder(correct);
  CHECK(result.status == CommandResult::Duplicate);
  CHECK(w.e.beginRuinsClue(0, 1) == CommandResult::InvalidPhase);
  std::puts("PASS order puzzle reports the first conflict, costs nothing and unlocks once");
}

static void ruinsTargetsAndSpecs() {
  RuinsWorld locked(false);
  CHECK(locked.e.selectRuinsTarget(144) == CommandResult::InvalidPhase);
  CHECK(locked.e.beginRuinsBoss(1) == CommandResult::InvalidPhase);
  for (uint8_t index = 0; index < kRuinsTargetTotal; ++index) {
    RuinsWorld w;
    const RuinsTargetSpec target = ruinsTargetAt(index);
    const uint64_t sequence = w.e.view().sequence;
    CHECK(w.e.selectRuinsTarget(target.species) == CommandResult::Applied);
    CHECK(w.e.view().ruins.lastTarget == target.species);
    CHECK(w.e.view().sequence == sequence + 1);
    CHECK(w.e.selectRuinsTarget(target.species) == CommandResult::Duplicate);
    CHECK(w.e.view().sequence == sequence + 1);
    CHECK(w.e.beginRuinsBoss(300 + index) == CommandResult::Applied);
    CHECK(w.e.view().phase == Phase::EncounterReady);
    CHECK(w.e.view().encounter.species == target.species &&
          w.e.view().encounter.opponent == target.species);
    CHECK(w.e.view().encounter.origin == EncounterOrigin::Ruins);
    CHECK(w.e.view().encounter.id == w.e.view().nextEncounterId - 1);
    CHECK(w.e.view().activityEncounterCount == 2);
    CHECK(w.e.view().explorationCredit == 0);
    // A second target cannot overwrite the open encounter.
    CHECK(w.e.selectRuinsTarget(ruinsTargetAt(uint8_t((index + 1) % kRuinsTargetTotal)).species) ==
          CommandResult::InvalidPhase);
    CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
    const BattleView view = w.e.battleView();
    CHECK(view.playerHp == 3 && view.playerMaxHp == 3);
    CHECK(view.enemyHp == target.battle.enemyHp && view.enemyMaxHp == target.battle.enemyHp);
    CHECK(view.beatMs == target.battle.beatMs);
    CHECK(view.normalWindowMs == 180 && view.perfectWindowMs == 70 && view.guard == 0);
    CHECK(view.expected == target.battle.expected(0));
    // Restarting mid-battle re-enters EncounterReady and restores the same spec.
    Engine reboot(w.store);
    CHECK(reboot.begin() == CommandResult::Applied);
    CHECK(reboot.view().phase == Phase::EncounterReady &&
          reboot.view().encounter.id == w.e.view().encounter.id);
    CHECK(reboot.startBattle(5000) == CommandResult::Applied);
    const BattleView again = reboot.battleView();
    CHECK(again.enemyMaxHp == target.battle.enemyHp && again.beatMs == target.battle.beatMs);
    CHECK(again.expected == target.battle.expected(0));
  }
  std::puts("PASS all eleven targets select once and fight with their catalog spec");
}

static void ruinsCoreCases() {
  ruinsCatalogTable();
  ruinsClueFlow();
  ruinsPermutations();
  ruinsCapturedClueOpponent();
  ruinsOrderPuzzle();
  ruinsTargetsAndSpecs();
}
