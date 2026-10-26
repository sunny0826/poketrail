#pragma once
#include "ruins_cases.hpp"

// Every catalog target must be beatable with its own rhythm and then reach the
// shared throw flow. Each target starts from a fresh save, earns the three clues
// and the unlock through the public commands, and captures once with a seed that
// leaves probability and roll untouched.
//
// The rows are printed before they are asserted so a failure on one boss still
// shows the whole field; a table that stopped at the first CHECK would hide the
// second unwinnable target.
struct RuinsBossRow {
  uint16_t species = 0;
  Result result = Result::None;
  Phase phase = Phase::Walking;
  uint8_t cues = 0;
  uint8_t enemyHp = 0;
  uint8_t usedBalls = 0;
  uint32_t captureCount = 0;
  uint16_t firstSpecies = 0;
  bool captured = false;
};

// Five earned balls and one unspent ordinary credit: the clue run never charges
// the credit, so nothing here fakes the activity ledger.
static Save ruinsFreshFundedSave() {
  Save s = ruinsLockedSave();
  s.totalSteps = s.undatedSteps = 5000;
  s.explorationCredit = 5000;
  CHECK(validate(s));
  return s;
}

static void ruinsUnlockThroughClues(RuinsWorld& w) {
  for (uint8_t index = 0; index < kRuinsClueTotal; ++index) {
    CHECK(w.e.beginRuinsClue(index, 400 + index) == CommandResult::Applied);
    CHECK(ruinsPlay(w) == Result::RuinsClue);
    ruinsClose(w);
  }
  uint8_t correct[3] = {1, 0, 2};
  CHECK(w.e.solveRuinsOrder(correct).status == CommandResult::Applied);
  CHECK(w.e.view().ruins.unlocked);
  CHECK(w.e.view().phase == Phase::Walking && w.e.view().encounter.id == 0);
}

static void ruinsBossWinnableTable() {
  RuinsBossRow rows[kRuinsTargetTotal];
  for (uint8_t index = 0; index < kRuinsTargetTotal; ++index) {
    const RuinsTargetSpec spec = ruinsTargetAt(index);
    RuinsWorld w(ruinsFreshFundedSave());
    ruinsUnlockThroughClues(w);
    CHECK(w.e.selectRuinsTarget(spec.species) == CommandResult::Applied);
    // A legal seed that makes the first real throw succeed; captureProbability
    // and throwRoll are read, never overridden.
    const uint32_t seed = ruinsSeed(w, spec.species, true, true);
    CHECK(w.e.beginRuinsBoss(seed) == CommandResult::Applied);
    const uint64_t ballsBefore = availableBalls(w.e.view());
    const uint64_t revisionBefore = w.e.view().rosterRevision;
    CHECK(w.e.view().activityEncounterCount == 4);

    rows[index].species = spec.species;
    rows[index].result = ruinsPlay(w);
    rows[index].phase = w.e.view().phase;
    rows[index].cues = w.e.battleView().cueIndex;
    rows[index].enemyHp = w.e.battleView().enemyHp;
    if (rows[index].result != Result::Won || rows[index].phase != Phase::ThrowReady) continue;

    // Only a real win reaches this block; the throw is the ordinary public path.
    CHECK(w.ball(1) == CommandResult::Applied);
    rows[index].captured = w.e.view().phase == Phase::Resolved && w.e.view().result == Result::Captured;
    rows[index].usedBalls = w.e.view().encounter.usedBalls;
    rows[index].captureCount = w.e.view().captureCounts[spec.species - 1];
    rows[index].firstSpecies = w.e.view().firstCapture[spec.species - 1].species;
    CHECK(w.e.view().phase == Phase::Resolved && w.e.view().result == Result::Captured);
    CHECK(rows[index].captureCount == 1);
    CHECK(rows[index].firstSpecies == spec.species);
    CHECK(rows[index].usedBalls == 1 && w.e.view().ballsSpent == 1);
    CHECK(availableBalls(w.e.view()) == ballsBefore - 1);
    // One capture owns the species once: the roster revision and the recent list
    // each move exactly one step.
    CHECK(w.e.view().rosterRevision == revisionBefore + 1);
    CHECK(w.e.view().recentSize == 1);
    CHECK(w.e.view().firstCapture[spec.species - 1].route == w.e.view().encounter.route);
    // The books survive a real reboot from the encoded bytes.
    Engine reboot(w.store);
    CHECK(reboot.begin() == CommandResult::Applied);
    CHECK(reboot.view().captureCounts[spec.species - 1] == 1);
    CHECK(reboot.view().firstCapture[spec.species - 1].species == spec.species);
    CHECK(reboot.view().ballsSpent == 1 && reboot.view().rosterRevision == revisionBefore + 1);
    CHECK(reboot.view().result == Result::Captured);
  }
  for (uint8_t index = 0; index < kRuinsTargetTotal; ++index) {
    const RuinsBossRow& row = rows[index];
    std::printf("TARGET %u result=%u phase=%u cues=%u enemyHp=%u captured=%u count=%u first=%u balls=%u\n",
                unsigned(row.species), unsigned(row.result), unsigned(row.phase), unsigned(row.cues),
                unsigned(row.enemyHp), unsigned(row.captured), unsigned(row.captureCount),
                unsigned(row.firstSpecies), unsigned(row.usedBalls));
  }
  for (uint8_t index = 0; index < kRuinsTargetTotal; ++index) {
    const RuinsBossRow& row = rows[index];
    CHECK(row.result == Result::Won);
    CHECK(row.phase == Phase::ThrowReady);
    CHECK(row.cues < 12);  // the 12-cue limit is a loss, never a win
    CHECK(row.enemyHp == 0);
    CHECK(row.captured && row.captureCount == 1 && row.firstSpecies == row.species);
    CHECK(row.usedBalls == 1);
  }
  std::puts("PASS all eleven bosses win before the cue limit and capture exactly once");
}

static void ruinsBossWinnableCases() { ruinsBossWinnableTable(); }
