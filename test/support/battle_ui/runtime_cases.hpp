#pragma once
#include "harness.hpp"

// Real Runtime/Button_Class battle surfaces: input, pause and commit failures.
namespace {
void attackAndDodge() {
  MemoryStore store; fixture(store); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r), target = r.engine().battleView().targetMs;
  at(r, target - 400); actor(4, origin, 155, 232, 2); actor(25, origin, 311, 226, 2);
  snapshot("battle-idle");
  action(r, Action::Attack, target - 100); const uint32_t settled = target - 100;
  check(r.engine().battleView().consumed && r.engine().battleView().lastTiming == Timing::Normal &&
        r.engine().battleView().enemyHp == 2, "normal input preserves one damage");
  hp(3, 2);
  at(r, settled + 100); actor(4, origin, 189, 232, 2); actor(25, origin, 311, 226, 2, true, theme::kText);
  snapshot("battle-attack-impact");
  at(r, settled + 300);
  check(r.engine().battleView().cueIndex == 1 && !r.engine().battleView().consumed, "next cue advances on original deadline");
  actor(4, origin, 166, 232, 2);
  snapshot("battle-attack-recovery");
  at(r, settled + 450); actor(4, origin, 155, 232, 2); actor(25, origin, 311, 226, 2); hp(3, 2);
  action(r, Action::Dodge, r.engine().battleView().targetMs - 100); const uint32_t dodge = host_time_ms;
  at(r, dodge + 120); hp(3, 2); actor(4, origin, 129, 232, 2);
  snapshot("battle-dodge-trails");
  at(r, dodge + 450); actor(4, origin, 155, 232, 2); actor(25, origin, 311, 226, 2);
  snapshot("battle-dodge-recovered");
}
void perfectAndFinal() {
  MemoryStore store; fixture(store, 4, 41, Route::Cave); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r), target = r.engine().battleView().targetMs;
  action(r, Action::Attack, target); hp(3, 1);
  at(r, target + 125); hp(3, 1); actor(4, origin, 188, 232, 2);
  snapshot("battle-perfect-impact");
  at(r, target + 450); actor(4, origin, 155, 232, 2); actor(41, origin, 311, 226, 2);
  action(r, Action::Dodge, r.engine().battleView().targetMs); at(r, host_time_ms + 450);
  const uint32_t final = r.engine().battleView().targetMs; const unsigned commits = store.commits;
  buttons(false, true); at(r, final);
  check(r.screen().page == Page::Battle && r.engine().view().phase == Phase::ThrowReady && store.commits == commits + 1,
        "final hit commits victory immediately while battle stays visible");
  Save saved; check(store.load(saved).status == LoadStatus::Loaded && saved.phase == Phase::ThrowReady, "victory already survives codec reload during final animation");
  at(r, final + 125); hp(3, 0); snapshot("battle-final-hit");
  at(r, final + 449); check(r.screen().page == Page::Battle, "final action remains until 449ms");
  at(r, final + 450); check(r.screen().page == Page::Throw && !r.engine().throwView().active, "450ms enters throw without reusing winning key");
  at(r, final + 600); check(!r.engine().throwView().active, "held winning key cannot start throw");
  buttons(false, false); frame(r); edge(r, false, true);
  check(r.engine().throwView().active && !r.engine().view().encounter.usedBalls, "fresh key prepares first ball without spending it");
}
void failureAndPractice() {
  for (unsigned practice = 0; practice < 2; ++practice) {
    MemoryStore store; fixture(store, 7, 54, Route::Coast); HostPlatform platform; Runtime r(store, platform); start(r);
    enter(r, practice); const unsigned commits = store.commits;
    for (unsigned cue = 0; cue < 6; ++cue) {
      const auto b = practice ? r.practiceView() : r.engine().battleView();
      at(r, b.targetMs + 181);
      if (cue == 1) { at(r, host_time_ms + 120); hp(2, 3); snapshot(practice ? "practice-miss" : "battle-miss"); }
    }
    const auto b = practice ? r.practiceView() : r.engine().battleView();
    check(!b.active && b.result == Result::Lost && b.playerHp == 0, "three failed dodges still end battle");
    check(r.screen().page == (practice ? Page::Practice : Page::Battle), "loss has a final presentation window");
    at(r, b.settledMs + 449); check(r.screen().page == (practice ? Page::Practice : Page::Battle), "loss remains before 450ms");
    at(r, b.settledMs + 450); check(r.screen().page == (practice ? Page::PracticeResult : Page::Result), "loss routes at 450ms");
    if (practice) check(store.commits == commits && r.engine().view().phase == Phase::EncounterReady, "practice grants nothing and retains encounter");
  }
  MemoryStore store; fixture(store); HostPlatform platform; Runtime r(store, platform); start(r); enter(r);
  action(r, Action::Attack, r.engine().battleView().targetMs); at(r, host_time_ms + 450);
  action(r, Action::Dodge, r.engine().battleView().targetMs); at(r, host_time_ms + 450);
  store.fail = true; action(r, Action::Attack, r.engine().battleView().targetMs);
  check(r.screen().page == Page::Error && r.engine().savePending() && r.engine().view().phase == Phase::Battle,
        "failed final commit takes priority over animation and never publishes victory");
  snapshot("battle-save-failure");
  store.fail = false; frame(r); select(r, 0);
  check(r.screen().page == Page::Throw && r.engine().view().phase == Phase::ThrowReady, "retry publishes the original victory once");
}
void timingAndPause() {
  Battle b; b.begin(UINT32_MAX - 1499);
  check(!battleEffectActive(b.view(), UINT32_MAX - 1499), "begin never plays an old effect");
  check(b.press(Action::Attack, 0), "settlement at millis zero is accepted");
  check(battleEffectActive(b.view(), 0) && battleEffectActive(b.view(), 449), "zero settlement supports complete effect");
  b.tick(181); check(battleEffectAction(b.view()) == Action::Attack && battleEffectActive(b.view(), 300), "effect preserves previous action across cue advance");
  check(!battleEffectActive(b.view(), 450), "effect ends exactly at 450ms");
  b.requestPause(); b.tick(b.view().targetMs + 181); const uint32_t pause = b.view().settledMs;
  check(!battleEffectActive(b.view(), pause), "paused battle suppresses effects");
  b.resume(pause + 1); check(!battleEffectActive(b.view(), pause + 1), "resume does not replay prior cue");
  b.begin(UINT32_MAX - 1700); b.press(Action::Attack, UINT32_MAX - 200); b.tick(50);
  check(battleEffectActive(b.view(), 100) && battleEffectAction(b.view()) == Action::Attack, "effect timing survives millis rollover");
  MemoryStore store; fixture(store, 17, 25); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r); at(r, r.engine().battleView().targetMs - 400);
  actor(17, origin, 155, 232, 1); actor(25, origin, 311, 226, 2); snapshot("battle-tall-partner");
  tap(r, 233, 30); at(r, r.engine().battleView().targetMs + 181);
  check(r.screen().page == Page::Pause, "real top touch pauses on cue boundary"); screenshot(r, "battle-paused");
  select(r, 0); check(r.screen().page == Page::Battle && !battleEffectActive(r.engine().battleView(), host_time_ms), "real resume begins a clean beat");
  snapshot("battle-resumed");
}
}
