#include "support/adventure_ui/harness.hpp"
#include "support/adventure_ui/clock_cases.hpp"
#include "support/adventure_ui/capture_cases.hpp"
#include "scene_backgrounds_generated.hpp"

namespace {
void frozenScene(Route route) {
  unsigned matched = 0;
  for (unsigned y = 180; y < 190; ++y) for (unsigned x = 70; x < 80; ++x) {
    const unsigned index = ((y - 92) / 2) * data::kSceneWidth + (x - 5) / 2;
    matched += M5.Display.readPixel(x, y) == data::kRouteScenesDim[uint8_t(route)].pixels[index];
  }
  check(matched == 100, "encounter background retains its saved route after selecting another route");
}
void bootAndCapture() {
  MemoryStore store; TestPlatform p; Runtime r(store, p); start(r);
  published(r, "first-boot");
  skipDateAndStarter(r, p);
  check(owns(r.engine().view(), 1) && r.engine().view().captureCounts[0] == 0, "starter owned without fake capture card");
  published(r, "home");
  openMenu(r, p); select(r, p, 1); select(r, p, 1);
  check(r.engine().view().route == Route::Coast, "route selection uses Engine command");
  openMenu(r, p); select(r, p, 2);
  check(menuCount(r.screen(), r.engine()) == 2, "partner menu only owns starter plus back");
  select(r, p, 1); select(r, p, 7);
  walk(r, p, 10008);
  check(r.engine().view().phase == Phase::EncounterReady && r.engine().view().totalSteps >= 10000, "continuous IMU steps create 5000-step encounter and bank next cycle");
  check(r.dimmed() && p.light == 0, "30-second inactivity dims but continues sampling");
  const auto total = r.engine().view().totalSteps;
  const auto beforePage = r.screen().page;
  p.next.power = true; frame(r); frame(r);
  check(!r.dimmed() && r.screen().page == beforePage && r.screen().selected == 0, "wake press does not navigate");
  check(p.feedbacks == 1, "one vibration per saved encounter");
  openMenu(r, p); select(r, p, 1); select(r, p, 2);
  check(r.engine().view().route == Route::Cave && r.engine().view().encounter.route == Route::Coast,
        "route selection preserves the existing encounter");
  openMenu(r, p); select(r, p, 0); published(r, "encounter"); frozenScene(Route::Coast);
  select(r, p, 1); check(r.screen().page == Page::Practice, "encounter offers free practice");
  tap(r, p, 233, 30); idle(r, 1800);
  check(r.screen().page == Page::Pause, "practice pauses after current cue");
  select(r, p, 1);
  check(r.engine().view().totalSteps == total && r.engine().view().phase == Phase::EncounterReady, "practice grants no rewards and consumes no encounter");
  reachBattle(r, p); published(r, "battle");
  tap(r, p, 233, 30); idle(r, 1800);
  check(r.screen().page == Page::Pause, "battle pauses after cue closes"); published(r, "pause");
  idle(r, 30020); check(r.dimmed(), "paused battle enters power saving without resuming");
  p.next.power = true; frame(r); frame(r);
  check(!r.dimmed() && r.screen().page == Page::Pause, "wake preserves paused battle");
  select(r, p, 0); check(r.screen().page == Page::Battle, "pause resumes");
  win(r, p);
  check(r.screen().page == Page::Battle, "committed victory keeps the final action visible before the throw menu");
  published(r, "throw-ready-1"); frozenScene(Route::Coast); frame(r);
  check(r.engine().throwView().active == false && r.engine().view().encounter.usedBalls == 0,
        "victory lands on the throw page with the window still closed");
  p.next.right = p.next.held = true; frame(r);
  check(r.engine().throwView().active && r.engine().view().encounter.usedBalls == 0, "prepare first ball consumes no ball");
  published(r, "throw-active");
  holdThrow(r, p, 1800);
  check(r.engine().view().encounter.usedBalls == 1 && !r.engine().throwView().active, "first timeout uses keyed attempt and waits for explicit second prepare");
  p.next.right = p.next.held = true; frame(r);
  published(r, "throw-ready-2");
  auto key = r.engine().throwView().key;
  store.fail = true; store.writeThenFail = true;
  throwAt(r, p);
  check(r.engine().savePending() && r.screen().page == Page::Error, "uncertain throw commit shows error before rewards");
  check(r.engine().view().encounter.usedBalls == 1, "failed second save does not publish attempt");
  frame(r); published(r, "save-retry");
  const auto rtcErrors = r.metrics().rtcErrors;
  p.rtcValid = false; idle(r, 1200);
  check(r.metrics().rtcErrors > rtcErrors, "RTC is still polled while frozen save awaits retry");
  p.rtcValid = true;
  store.fail = false; select(r, p, 0);
  check(r.engine().view().encounter.usedBalls == 2 && r.screen().page == Page::Result, "retry publishes same saved second attempt");
  check(r.engine().view().encounter.id == key.encounterId, "retry preserves encounter identity");
  published(r, "result");
  if (r.engine().view().result == Result::Captured) {
    check(r.engine().view().recentSize == 1, "capture awards exactly one recent record");
    const auto& c = r.engine().view().recent[0];
    check(!c.at.timeKnown && c.at.totalSteps == r.engine().view().totalSteps, "capture records actual unknown time and step count");
    screenshot("capture-card");
  }
  select(r, p, 0); check(r.screen().page == Page::Home, "acknowledge result returns home");
  check(r.engine().view().phase == Phase::EncounterReady && r.engine().view().encounter.id != key.encounterId,
        "result acknowledgement immediately redeems saved credit without extra steps");
  openMenu(r, p); select(r, p, 4); published(r, "resources"); select(r, p, 0);
  published(r, "records"); select(r, p, 0); published(r, "first-capture");
  select(r, p, 150); check(r.screen().selected == 150, "permanent Kanto 151st species reachable independently of recent ring");
  published(r, "first-capture-151"); tap(r, p, 233, 360);
  select(r, p, 0); select(r, p, 250); check(r.screen().selected == 250, "permanent Johto 251st species reachable in the extended catalog");
  published(r, "first-capture-251"); tap(r, p, 233, 360);
  select(r, p, 0); select(r, p, 648);
  check(r.screen().selected == 648, "permanent Unova 649th species reachable in the extended catalog");
  published(r, "first-capture-649"); tap(r, p, 233, 360);
  select(r, p, 1); published(r, "recent"); tap(r, p, 233, 360);
  select(r, p, 2); published(r, "days"); tap(r, p, 233, 360);
  select(r, p, 3); published(r, "achievements"); tap(r, p, 233, 360);
  tap(r, p, 233, 30);
  frame(r);
  tap(r, p, 233, 30);
  frame(r);
  tap(r, p, 233, 30);
  frame(r);
  openMenu(r, p);
  select(r, p, 3);
  check(r.screen().page == Page::Viewer, "real menu enters legacy viewer");
  screenshot("viewer-entry");
  tap(r, p, 233, 30); check(r.screen().page == Page::Home, "viewer top press returns without giving ownership");
  check(r.metrics().samples > 12000 && r.metrics().maxSampleGapMs >= 500 && r.metrics().qualityGaps > 0, "runtime measures samples and long gaps");
}
void fullRecordRestart() {
  MemoryStore store; Save save;
  save.schema = 1; save.bootGeneration = 1; save.starter = save.partner = 1;
  save.encounter.ruleVersion = 1;
  for (auto& attempt : save.encounter.attempts) attempt.ruleVersion = 1;
  setOwned(save, 1); setOwned(save, 25); setOwned(save, 151);
  save.totalSteps = 17000; save.undatedSteps = 9500; save.nextEncounterId = 35;
  save.clock.calibrated = true; save.clock.dayHighWater = 20261022;
  save.today.dayKey = 20261022; save.today.steps = 500; save.bestDaySteps = 500;
  save.historySize = 14;
  for (unsigned i = 0; i < 14; ++i) { save.history[i].dayKey = 20261008 + i; save.history[i].steps = 500; }
  Capture first; first.species = 151; first.at.bootGeneration = 1; first.at.totalSteps = 500;
  save.firstCapture[150] = first; save.captureCounts[150] = 1;
  first.species = 25; first.at.totalSteps = 1000;
  save.firstCapture[24] = first; save.captureCounts[24] = 33;
  save.recentSize = 32;
  for (unsigned i = 0; i < 32; ++i) {
    first.at.totalSteps = 1500 + 500 * i; save.recent[i] = first;
  }
  save.lifetime[0].reached = true; save.lifetime[0].first.bootGeneration = 1;
  save.lifetime[0].first.totalSteps = 10000;
  check(validate(save), "ring eviction restart fixture passes domain semantic validator");
  check(store.commit(save) == CommitStatus::Committed, "restart fixture is encoded by production codec");
  TestPlatform p; Runtime r(store, p); start(r);
  openMenu(r, p); select(r, p, 4); select(r, p, 0); select(r, p, 0); select(r, p, 150);
  check(r.engine().view().firstCapture[150].species == 151 && r.engine().view().recent[0].species == 25,
        "permanent record remains available after recent capture eviction");
  published(r, "permanent-after-ring-eviction");
  tap(r, p, 233, 360); select(r, p, 1); select(r, p, 31);
  published(r, "recent-page-32"); tap(r, p, 233, 360);
  select(r, p, 2); select(r, p, 14); published(r, "history-page-14");
  check(r.engine().view().history[0].dayKey == 20261008, "oldest of fourteen daily records remains reachable");
  tap(r, p, 233, 360);
  select(r, p, 3); select(r, p, 3); published(r, "lifetime-earned");
}

void viewerWakeAndPractice() {
  MemoryStore store; TestPlatform p; Runtime r(store, p); start(r); skipDateAndStarter(r, p);
  openMenu(r, p); tap(r, p, 298, 430);
  M5.Touch.detail.base_x = M5.Touch.detail.x = 233;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 150;
  M5.Touch.detail.pressed = true;
  p.next.touch = p.next.held = true; p.next.x = 233; p.next.y = 150; frame(r);
  check(r.screen().page == Page::Menu && r.screen().selected == 3,
        "visible viewer button waits for click release");
  M5.Touch.detail.pressed = false; M5.Touch.detail.released = M5.Touch.detail.clicked = true;
  p.next.click = true; p.next.x = 233; p.next.y = 150; frame(r);
  check(r.screen().page == Page::Viewer, "visible menu click release enters real viewer");
  M5.Touch.detail = HostTouchDetail(); idle(r, 240);
  check(p.feedbacks == 0 && M5.Display.readPixel(410, 154) == theme::kViewerBackground, "entry touch release cannot trigger viewer reaction");
  idle(r, 30000); check(r.dimmed(), "viewer also dims without stopping runtime");
  M5.Touch.detail.base_x = M5.Touch.detail.x = 233;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 233;
  M5.Touch.detail.pressed = true;
  p.next.touch = p.next.held = true; p.next.x = p.next.y = 233; frame(r);
  check(r.dimmed(), "touch leaves the viewer locked");
  p.next.power = p.next.held = true; frame(r);
  M5.Touch.detail.pressed = false; M5.Touch.detail.released = M5.Touch.detail.clicked = true; frame(r);
  M5.Touch.detail = HostTouchDetail();
  check(!r.dimmed() && p.feedbacks == 0, "wake click release cannot leak into viewer interaction");
  M5.Touch.detail.base_x = M5.Touch.detail.x = 233;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 233;
  M5.Touch.detail.released = M5.Touch.detail.clicked = true; frame(r);
  M5.Touch.detail = HostTouchDetail();
  check(p.feedbacks == 1, "subsequent real viewer tap uses runtime feedback wrapper");
  M5.Touch.detail.base_x = 300; M5.Touch.detail.x = 170;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 30;
  M5.Touch.detail.released = true; frame(r); M5.Touch.detail = HostTouchDetail();
  check(r.screen().page == Page::Viewer && p.feedbacks == 2, "horizontal swipe at top remains a viewer swipe");
  p.next.pause = true; frame(r);
  check(r.screen().page == Page::Viewer, "long dual-button gesture does not accidentally leave viewer");
  tap(r, p, 233, 30); openMenu(r, p); select(r, p, 5);
  check(r.screen().page == Page::Practice, "home free practice reachable");
  idle(r, 240);
  unsigned commits = store.commits; uint64_t steps = r.engine().view().totalSteps;
  walk(r, p, 3);
  check(store.commits == commits && r.engine().view().totalSteps == steps, "practice timing defers step commits and clock checkpoints");
  tap(r, p, 233, 30); idle(r, 1600);
  check(r.screen().page == Page::Pause && r.engine().view().totalSteps == steps + 3,
        "practice pause flushes original confirmed steps at a safe point");
  select(r, p, 1); openMenu(r, p); select(r, p, 1);
  store.delayMs = 125; select(r, p, 2); store.delayMs = 0; frame(r);
  check(r.metrics().maxCommitMs == 125 && r.metrics().maxSampleGapMs >= 125,
        "commit latency and its sample gap are observed, not hidden by timer catch-up");
}

}
int main() {
  c152::begin("ADVENTURE TEST");
  bootAndCapture(); clockAndErrors(); midnightAndSampling(); slowGaitAttribution(); stallAttribution(); practiceMidnightAchievement();
  fullRecordRestart(); viewerWakeAndPractice(); firstBallSuccess(); captureAnimation(); captureEscape();
  std::printf("%u adventure UI checks, 0 failures\n", checks);
}
