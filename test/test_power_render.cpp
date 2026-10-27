#include "support/power_fixture.hpp"

namespace {
void powerClick(Runtime& runtime) { M5.powerClick(); frame(runtime); }
void powerWake(Runtime& runtime) {
  frame(runtime); powerClick(runtime); idle(runtime, 160); frame(runtime);  // a frame without a press separates the presses
  check(!runtime.dimmed(), "power click wakes after settling and release");
}
void powerButton() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::Home && !runtime.dimmed(), "power button test starts on home");
  powerClick(runtime);
  check(runtime.dimmed() && platform.sleeps == 1 && platform.wakes == 0 && M5.Display.brightness == 0,
        "power click sleeps home and blanks the display");
  check(runtime.screen().page == Page::Home && runtime.screen().selected == 0 && !runtime.screen().editing,
        "power click leaves the home page untouched");
  frame(runtime);
  check(runtime.dimmed() && platform.sleeps == 1 && platform.wakes == 0,
        "one completed click cannot wake the display it just slept");
  powerClick(runtime);
  check(runtime.dimmed() && platform.wakes == 1, "next power click starts a wake");
  idle(runtime, 140);
  check(runtime.dimmed() && M5.Display.brightness == 0, "power wake remains dark before the settle");
  frame(runtime, 10);
  check(!runtime.dimmed() && platform.wakes == 1 && runtime.screen().page == Page::Home &&
        runtime.screen().selected == 0, "power wake lights without a page action");
  frame(runtime); check(!runtime.dimmed() && platform.sleeps == 1, "the wake click cannot re-sleep after wake");
  powerClick(runtime);
  check(runtime.dimmed() && platform.sleeps == 2, "a released power click sleeps again");
}
void powerDuringWake() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); for (unsigned i = 0; i < 3; ++i) edge(runtime, false, true);
  idle(runtime, 30020);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3 && runtime.dimmed() &&
        platform.sleeps == 1 && platform.wakes == 0, "wake regression sleeps on the selected menu item");
  edge(runtime, false, true); idle(runtime, 120);
  check(runtime.dimmed() && platform.wakes == 1, "wake regression is still settling at 140ms");
  powerClick(runtime);
  check(!runtime.dimmed() && M5.Display.brightness == 96 && platform.sleeps == 1 && platform.wakes == 1 &&
        runtime.screen().page == Page::Menu && runtime.screen().selected == 3,
        "power click on the wake-completion frame cannot re-sleep or reach the page");
}
void powerClickSequence() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  M5.powerClick(); M5.powerClick(); frame(runtime);
  check(runtime.dimmed() && platform.sleeps == 1 && platform.wakes == 0, "duplicate presses in one frame are one event");
  M5.powerClick(); frame(runtime); check(runtime.dimmed() && platform.wakes == 1, "a second press in the next frame wakes");
  idle(runtime, 160);
  check(!runtime.dimmed() && platform.wakes == 1 && platform.sleeps == 1, "the adjacent press wakes only");
  frame(runtime); powerClick(runtime);
  check(runtime.dimmed() && platform.sleeps == 2, "the next separated press sleeps again");
  idle(runtime, 700); check(runtime.dimmed() && platform.sleeps == 2 && platform.wakes == 1, "no stale press after a long idle");
}
void powerMenu() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime);
  for (unsigned i = 0; i < 3; ++i) edge(runtime, false, true);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3, "menu selection is reachable");
  powerClick(runtime);
  check(runtime.dimmed() && platform.sleeps == 1 && runtime.screen().page == Page::Menu &&
        runtime.screen().selected == 3, "power click sleeps without confirming the menu item");
  powerWake(runtime);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3,
        "power wake leaves menu selection untouched");
}
void powerBlocked() {
  {
    MemoryStore store; ready(store, 5000);
    PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
    check(runtime.engine().view().phase == Phase::EncounterReady, "battle fixture is ready");
    openMenu(runtime); select(runtime, 0); select(runtime, 0); frame(runtime);
    check(runtime.screen().page == Page::Battle && runtime.engine().battleView().active,
          "formal battle is active");
    powerClick(runtime);
    check(!runtime.dimmed() && platform.sleeps == 0, "power click cannot sleep an active battle");
  }
  {
    MemoryStore store; ready(store);
    PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
    openMenu(runtime); select(runtime, 5);
    check(runtime.screen().page == Page::Practice, "practice is active");
    powerClick(runtime);
    check(!runtime.dimmed() && platform.sleeps == 0, "power click cannot sleep active practice");
  }
  {
    MemoryStore store; Save save;
    save.bootGeneration = 1; save.starter = save.partner = 1; setOwned(save, 1); setRegistered(save, 1);
    save.rosterRevision = 1; save.encounterTarget = 300;
    save.totalSteps = save.undatedSteps = 5000; save.explorationSpent = 5000; save.nextEncounterId = 2;
    save.phase = Phase::ThrowReady; save.result = Result::Won;
    save.encounter.id = 1; save.encounter.species = save.encounter.partner = 1;
    check(validate(save) && store.commit(save) == CommitStatus::Committed, "throw fixture validates");
    PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
    edge(runtime, false, true);
    check(runtime.engine().throwView().active, "throw timing is active");
    powerClick(runtime);
    check(!runtime.dimmed() && platform.sleeps == 0, "power click cannot sleep an active throw");
    while (runtime.engine().throwView().active) frame(runtime);
    check(!runtime.dimmed(), "throw timeline finishes without a power sleep");
  }
  {
    MemoryStore store; legacyOwned(store, {1}, 1);
    { Engine engine(store); check(engine.begin() == CommandResult::Applied, "evolution fixture migrates");
      const auto view = engine.evolutionView(1); check(view.count > 0, "evolution fixture has an option");
      step(engine, 1, view.options[0].cost + 500); engine.checkpoint(); }
    PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
    openMenu(runtime); select(runtime, 2); select(runtime, 0); select(runtime, 1);
    check(runtime.screen().page == Page::EvolutionConfirm, "evolution confirmation is reachable");
    buttons(true, true); frame(runtime); buttons(false, false); frame(runtime);
    check(runtime.screen().page == Page::Evolution, "evolution animation is active");
    powerClick(runtime);
    check(!runtime.dimmed() && platform.sleeps == 0, "power click cannot sleep an evolution animation");
  }
}
void powerSafePages() {
  {
    MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
    openMenu(runtime); select(runtime, 5);
    for (unsigned elapsed = 0; elapsed < 6500; elapsed += 20) frame(runtime);
    tap(runtime, 233, 30); idle(runtime, 1700);
    check(runtime.screen().page == Page::Pause && runtime.practiceView().paused, "practice reaches pause");
    const auto paused = runtime.practiceView();
    powerClick(runtime);
    check(runtime.dimmed() && platform.sleeps == 1, "power click sleeps a paused practice");
    powerWake(runtime);
    check(runtime.screen().page == Page::Pause && runtime.practiceView().paused &&
          runtime.practiceView().cueIndex == paused.cueIndex &&
          runtime.practiceView().playerHp == paused.playerHp,
          "power wake preserves the paused battle");
  }
  {
    MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
    openMenu(runtime); select(runtime, 4); select(runtime, 1); select(runtime, 0); edge(runtime, false, true);
    const auto draft = runtime.screen(); const unsigned writes = platform.clockWrites;
    check(draft.page == Page::Clock && draft.editing, "clock editor is open with a draft");
    powerClick(runtime);
    check(runtime.dimmed() && platform.sleeps == 1, "power click sleeps the clock editor");
    powerWake(runtime);
    const auto& after = runtime.screen();
    check(after.page == Page::Clock && after.editing && after.selected == draft.selected &&
          after.civil.year == draft.civil.year && after.civil.month == draft.civil.month &&
          after.civil.day == draft.civil.day && after.offset == draft.offset && after.clockMode == draft.clockMode,
          "power wake preserves the clock draft");
    check(platform.clockWrites == writes && !runtime.engine().view().clock.pending.active,
          "power sleep and wake never persist a clock draft");
  }
  {
    MemoryStore store; Save save;
    save.bootGeneration = 1; save.starter = save.partner = 1; setOwned(save, 1); setRegistered(save, 1);
    save.rosterRevision = 1; save.encounterTarget = 300;
    save.totalSteps = save.undatedSteps = 5000; save.explorationSpent = 5000; save.nextEncounterId = 2;
    save.phase = Phase::ThrowReady; save.result = Result::Won;
    save.encounter.id = 1; save.encounter.species = save.encounter.partner = 1;
    check(validate(save) && store.commit(save) == CommitStatus::Committed, "pending victory fixture validates");
    PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
    check(runtime.screen().page == Page::Throw, "waiting victory is visible");
    const uint64_t balls = availableBalls(runtime.engine().view());
    powerClick(runtime);
    check(runtime.dimmed() && platform.sleeps == 1, "power click sleeps a waiting throw");
    powerWake(runtime);
    check(runtime.screen().page == Page::Throw && !runtime.engine().throwView().active &&
          availableBalls(runtime.engine().view()) == balls && runtime.engine().view().encounter.id == 1,
          "power wake preserves victory without preparing or spending a ball");
  }
}
void powerFailure() {
  MemoryStore store; ready(store); PowerPlatform platform; platform.failSleep = true;
  Runtime runtime(store, platform); start(runtime);
  powerClick(runtime);
  check(!runtime.dimmed() && platform.sleeps == 1 && M5.Display.brightness == 96 &&
        runtime.metrics().displayErrors == 1, "rejected power sleep restores the visible UI");
  platform.failSleep = false;
  frame(runtime);  // next update is nochange, so the retry is a fresh click
  powerClick(runtime);
  check(runtime.dimmed() && platform.sleeps == 2, "a fresh power click retries a rejected sleep");
}
void powerHold() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  M5.powerDown(true); frame(runtime); check(runtime.dimmed() && platform.sleeps == 1, "holding the power key sleeps once");
  for (unsigned i = 0; i < 20; ++i) frame(runtime);
  check(runtime.dimmed() && platform.sleeps == 1 && platform.wakes == 0, "a held key does not repeat the press");
  M5.powerDown(false); frame(runtime); idle(runtime, 160); frame(runtime);
  check(runtime.dimmed() && platform.wakes == 0, "releasing a held key does not wake");
}
void powerWraparound() {
  host_time_ms = UINT32_MAX - 100;
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  powerClick(runtime);
  check(runtime.dimmed() && platform.sleeps == 1, "power sleep works across millis wrap");
  powerWake(runtime);
  check(host_time_ms < 1000 && platform.wakes == 1, "power wake settles across millis wrap");
  idle(runtime, 500); powerClick(runtime);
  check(runtime.dimmed() && platform.sleeps == 2, "power sleep after wrap does not reuse a stale click");
}
}
int main() {
  c152::begin("POWER TEST");
  powerButton(); powerDuringWake(); powerClickSequence(); powerMenu(); powerBlocked(); powerSafePages();
  powerHold(); powerFailure(); powerWraparound();
  std::printf("%u power Runtime/Button_Class/pixel checks, 0 failures\n", checks);
}
