#pragma once
#include "harness.hpp"

// Step accounting, route lock, replay and boot generation.
static void basicSteps() {
  World w;
  CHECK(w.e.chooseStarter(4) == CommandResult::InvalidPhase);
  CHECK(w.e.chooseStarter(1) == CommandResult::Duplicate);
  CHECK(w.e.selectPartner(151) == CommandResult::InvalidInput);
  CHECK(w.e.view().captureCounts[0] == 0 && w.e.view().firstCapture[0].species == 0);
  { Encounter legacy; legacy.species = 25; CHECK(encounterOpponent(legacy) == 25); }
  w.steps(4999); CHECK(w.e.view().phase == Phase::Walking);
  w.steps(1); CHECK(w.e.view().phase == Phase::EncounterReady && w.e.view().explorationCredit == 0);
  const Encounter target = w.e.view().encounter;
  CHECK(target.origin == EncounterOrigin::Ordinary && target.opponent == target.species &&
        w.e.view().activityEncounterCount == 0);
  w.steps(11000); CHECK(w.e.view().totalSteps == 16000 && w.e.view().explorationCredit == 11000);
  CHECK(w.e.view().undatedSteps == 16000 && w.e.view().today.steps == 0);
  CHECK(w.e.selectRoute(Route::Coast) == CommandResult::Applied);
  CHECK(w.e.view().encounter.species == target.species && w.e.view().encounter.route == target.route);
  StepEvent replay; replay.batchId = 16000; replay.clock = w.clock();
  CHECK(w.e.observeSteps(&replay, 1, 9) == CommandResult::Duplicate);
  CHECK(w.e.retreat() == CommandResult::Applied);
  CHECK(w.e.acknowledgeResult(target.id, 123) == CommandResult::Applied);
  CHECK(w.e.view().explorationCredit == 6000 && w.e.view().encounter.id == 2 && w.e.view().encounter.route == Route::Coast);
  CHECK(w.e.view().encounter.origin == EncounterOrigin::Ordinary &&
        w.e.view().encounter.opponent == w.e.view().encounter.species &&
        w.e.view().activityEncounterCount == 0);
  CHECK(w.e.pressThrow(AttemptKey(target.id, 1), 0, w.clock()) == CommandResult::InvalidInput);
  CHECK(w.e.checkpoint() == CommandResult::Duplicate);
  Engine reboot(w.store); CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().bootGeneration == 2 && reboot.view().lastStepBatch == 0);
  StepEvent fresh; fresh.batchId = 1; fresh.clock = reading(reboot, 0, 10, false);
  CHECK(reboot.observeSteps(&fresh, 1, 9) == CommandResult::Applied && reboot.view().totalSteps == 16001);
  CHECK(reboot.observeSteps(&replay, 1, 9) == CommandResult::InvalidInput);
  std::puts("PASS steps 4999/5000/16000, route lock, ownership, replay and boot generation");
}
