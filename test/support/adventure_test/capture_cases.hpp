#pragma once
#include "harness.hpp"

// Throw accounting, permanent first captures, recent ring and probability edges.
static void throwsAndRecords() {
  World w(true); w.steps(5000, seedFor(w.e.view(), false, true)); w.win();
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
    w.steps(5000, seed); w.win(); CHECK(w.ball(1) == CommandResult::Applied);
    CHECK(w.e.beginThrow(AttemptKey(w.e.view().encounter.id, 2), w.now) == CommandResult::InvalidPhase);
  }
  CHECK(w.e.view().recentSize == 32 && w.e.view().captureCounts[species - 1] == 1);
  unsigned totalCaptures = 0;
  for (auto count : w.e.view().captureCounts) { CHECK(count <= 1); totalCaptures += count; }
  CHECK(totalCaptures == 34);
  CHECK(w.e.view().firstCapture[species - 1].at.utcSeconds == first.utcSeconds);
  CHECK(w.e.view().firstCapture[species - 1].at.totalSteps == first.totalSteps);
  CHECK(w.e.view().lifetime[0].reached && w.e.view().dailyFirst[2].reached);
  World miss; miss.steps(5000); miss.win();
  CHECK(miss.ball(1, Timing::Miss) == CommandResult::Applied);
  CHECK(miss.e.beginThrow(AttemptKey(miss.e.view().encounter.id, 2), miss.now) == CommandResult::Applied);
  miss.now = miss.e.throwView().targetMs + 181;
  CHECK(miss.e.tick(miss.now, miss.clock(false)) == CommandResult::Applied);
  CHECK(miss.e.view().result == Result::Escaped && miss.e.view().recentSize == 0);
  World unknown; unknown.steps(5000, seedFor(unknown.e.view(), true, true, 1)); unknown.win();
  CHECK(unknown.ball(1) == CommandResult::Applied);
  CHECK(unknown.e.view().captureCounts[0] == 1 && !unknown.e.view().firstCapture[0].at.timeKnown);
  CHECK(unknown.e.view().firstCapture[0].at.bootGeneration == unknown.e.view().bootGeneration);
  CHECK(unknown.e.view().firstCapture[0].at.monotonicMs == unknown.now);
  unknown.calibrate(); CHECK(!unknown.e.view().firstCapture[0].at.timeKnown);
  CHECK(throwTiming(1120, 1000) == Timing::Nice && throwTiming(1121, 1000) == Timing::Normal);
  std::puts("PASS two throws, deduplication, capture atomic fields, permanent first and recent32");
}
static void probabilityBoundaries() {
  const uint8_t probabilities[2][3] = {{55,70,85},{30,45,60}};
  const Timing timings[] = {Timing::Normal, Timing::Nice, Timing::Perfect};
  for (unsigned rare = 0; rare < 2; ++rare) for (unsigned timing = 0; timing < 3; ++timing) {
    for (unsigned side = 0; side < 2; ++side) {
      World w;
      const uint8_t roll = uint8_t(probabilities[rare][timing] - 1 + side);
      uint32_t seed = 0;
      for (; seed < 1000000; ++seed) {
        Encounter e; e.id = 1; e.seed = seed; e.species = chooseEncounter(w.e.view(), Route::Forest, seed);
        if (catalogEntry(e.species)->rare == bool(rare) && throwRoll(e, 1) == roll) break;
      }
      CHECK(seed < 1000000); w.steps(5000, seed); w.win();
      CHECK(w.ball(1, timings[timing]) == CommandResult::Applied);
      const ThrowAttempt& a = w.e.view().encounter.attempts[0];
      CHECK(a.probability == probabilities[rare][timing] && a.roll == roll && a.captured == !side);
    }
  }
  std::puts("PASS all six probability thresholds at p-1 and p using raw press timestamps");
}
