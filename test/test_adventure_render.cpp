#include "support/adventure_ui/harness.hpp"
#include "support/adventure_ui/clock_cases.hpp"

namespace {
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
  select(r, p, 1);
  walk(r, p, 10008);
  check(r.engine().view().phase == Phase::EncounterReady && r.engine().view().totalSteps >= 10000, "continuous IMU steps create 5000-step encounter and bank next cycle");
  check(r.dimmed() && p.light == 0, "30-second inactivity dims but continues sampling");
  const auto total = r.engine().view().totalSteps;
  const auto beforePage = r.screen().page;
  edge(r, p, false, true);
  check(!r.dimmed() && r.screen().page == beforePage && r.screen().selected == 0, "wake press does not navigate");
  check(p.feedbacks == 1, "one vibration per saved encounter");
  openMenu(r, p); select(r, p, 0); published(r, "encounter");
  select(r, p, 1); check(r.screen().page == Page::Practice, "encounter offers free practice");
  tap(r, p, 233, 30); idle(r, 1800);
  check(r.screen().page == Page::Pause, "practice pauses after current cue");
  select(r, p, 1);
  check(r.engine().view().totalSteps == total && r.engine().view().phase == Phase::EncounterReady, "practice grants no rewards and consumes no encounter");
  reachBattle(r, p); published(r, "battle");
  tap(r, p, 233, 30); idle(r, 1800);
  check(r.screen().page == Page::Pause, "battle pauses after cue closes"); published(r, "pause");
  idle(r, 30020); check(r.dimmed(), "paused battle enters power saving without resuming");
  edge(r, p, false, true);
  check(!r.dimmed() && r.screen().page == Page::Pause, "wake preserves paused battle");
  select(r, p, 0); check(r.screen().page == Page::Battle, "pause resumes");
  win(r, p); published(r, "throw-ready-1");
  edge(r, p, false, true); check(r.engine().throwView().active && r.engine().view().encounter.usedBalls == 0, "prepare first ball consumes no ball");
  published(r, "throw-active");
  idle(r, 1800);
  check(r.engine().view().encounter.usedBalls == 1 && !r.engine().throwView().active, "first timeout uses keyed attempt and waits for explicit second prepare");
  published(r, "throw-ready-2");
  edge(r, p, false, true);
  auto key = r.engine().throwView().key;
  store.fail = true; store.writeThenFail = true;
  throwAt(r, p, r.engine().throwView().targetMs);
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
  openMenu(r, p); select(r, p, 4); published(r, "records"); select(r, p, 0); published(r, "first-capture");
  select(r, p, 150); check(r.screen().selected == 150, "permanent 151st species reachable independently of recent ring");
  published(r, "first-capture-151"); tap(r, p, 233, 360);
  select(r, p, 1); published(r, "recent"); tap(r, p, 233, 360);
  select(r, p, 2); published(r, "days"); tap(r, p, 233, 360);
  select(r, p, 3); published(r, "achievements"); tap(r, p, 233, 360);
  select(r, p, 4); openMenu(r, p); select(r, p, 3);
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
  openMenu(r, p); select(r, p, 4); select(r, p, 0); select(r, p, 150);
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
  openMenu(r, p);
  M5.Touch.detail.base_x = M5.Touch.detail.x = 233;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 372;
  M5.Touch.detail.pressed = true;
  p.next.touch = p.next.held = true; p.next.x = 233; p.next.y = 372; frame(r);
  check(r.screen().page == Page::Viewer, "visible menu touch enters real viewer");
  M5.Touch.detail.pressed = false; M5.Touch.detail.released = M5.Touch.detail.clicked = true; frame(r);
  M5.Touch.detail = HostTouchDetail(); idle(r, 240);
  check(p.feedbacks == 0 && M5.Display.readPixel(415, 200) == TFT_BLACK, "entry touch release cannot trigger viewer reaction");
  idle(r, 30000); check(r.dimmed(), "viewer also dims without stopping runtime");
  M5.Touch.detail.base_x = M5.Touch.detail.x = 233;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 233;
  M5.Touch.detail.pressed = true;
  p.next.touch = p.next.held = true; p.next.x = p.next.y = 233; frame(r);
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
  tap(r, p, 233, 30); openMenu(r, p); select(r, p, 6);
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
void savedVictory(MemoryStore& store, Save& s, bool captured) {
  s.bootGeneration = 1; s.starter = s.partner = 1; setOwned(s, 1); setRegistered(s, 1);
  s.rosterRevision = 1; s.totalSteps = 5000; s.undatedSteps = 4500; s.nextEncounterId = 2;
  s.clock.calibrated = true; s.clock.offsetMinutes = 480; s.clock.dayHighWater = 20261022;
  s.today.dayKey = 20261022; s.today.steps = s.bestDaySteps = 500;
  s.phase = Phase::ThrowReady; s.result = Result::Won;
  s.encounter.id = 1; s.encounter.species = s.encounter.partner = 1;
  while ((throwRoll(s.encounter, 1) >= captureProbability(catalogEntry(1)->rare, Timing::Perfect)) == captured)
    ++s.encounter.seed;
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "saved victory fixture validates and encodes");
}
void firstBallSuccess() {
  MemoryStore store; Save s;
  savedVictory(store, s, true);
  TestPlatform p; Runtime r(store, p); start(r);
  check(r.screen().page == Page::Throw && !r.engine().throwView().active, "saved victory restarts at explicit preparation");
  edge(r, p, false, true);
  throwAt(r, p, r.engine().throwView().targetMs);
  check(r.engine().view().result == Result::Captured && r.engine().view().encounter.usedBalls == 1 &&
        !r.engine().view().encounter.attempts[1].used, "first successful ball ends encounter with second unused");
  check(r.screen().page == Page::Throw && r.screen().capture.active,
        "committed capture plays the resolution animation on the throw page first");
  edge(r, p, false, true); edge(r, p, false, true); frame(r);
  check(r.screen().page == Page::Result && !r.screen().capture.active,
        "skipped animation settles on the result page");
  edge(r, p, false, true);
  check(r.engine().view().recentSize == 1 && r.engine().view().encounter.usedBalls == 1,
        "repeated right press on result cannot throw a second ball or duplicate capture");
  check(r.engine().view().recent[0].at.timeKnown && r.engine().view().recent[0].at.offsetMinutes == 480,
        "capture card uses verified current clock and original timezone offset");
  published(r, "first-ball-capture");
}
void captureAnimation() {
  {
    CaptureAnim probe; probe.active = true; probe.captured = false; probe.wiggleBreak = 2;
    check(captureFrameAt(probe, 449).stage == CaptureStage::Flight &&
          captureFrameAt(probe, 450).stage == CaptureStage::Flash &&
          captureFrameAt(probe, 570).stage == CaptureStage::Drop, "flight, flash and drop stages follow the timeline");
    check(captureFrameAt(probe, 1070).stage == CaptureStage::Wiggle &&
          captureFrameAt(probe, 1070 + 500 + 249).stage == CaptureStage::Wiggle &&
          captureFrameAt(probe, 1070 + 500 + 250).stage == CaptureStage::Reveal,
          "failed ball breaks out in the middle of its roll-chosen wiggle");
    probe.captured = true;
    check(captureFrameAt(probe, 1070 + 1499).stage == CaptureStage::Wiggle &&
          captureFrameAt(probe, 1070 + 1500).stage == CaptureStage::Reveal &&
          captureFrameAt(probe, captureTotalMs(probe)).stage == CaptureStage::Done,
          "captured ball completes three wiggles before the celebration");
    int x, y;
    captureFlightPos(0, x, y); check(x == 233 && y == 344, "flight starts at the held ball");
    captureFlightPos(1, x, y); check(x == 233 && y == 233, "flight lands on the species sprite");
    CaptureAnim skipped; skipped.active = true; skipped.captured = true;
    captureSkip(skipped, 100);
    check(captureFrameAt(skipped, 100).stage == CaptureStage::Reveal, "first skip jumps to the reveal");
    captureSkip(skipped, 200);
    check(captureFrameAt(skipped, 200).stage == CaptureStage::Done, "second skip finishes the animation");
  }
  MemoryStore store; Save s;
  savedVictory(store, s, true);
  TestPlatform p; Runtime r(store, p); start(r);
  edge(r, p, false, true);
  const uint32_t target = r.engine().throwView().targetMs;
  p.next.right = p.next.held = true; frame(r);
  check(r.screen().charging, "holding the throw key charges the ball");
  for (unsigned held = 0; held < 920; held += 20) { p.next.rightDown = p.next.held = true; frame(r); }
  p.next.rightDown = p.next.held = true; frame(r);
  check(M5.Display.readPixel(267, 344) == kGold || M5.Display.readPixel(263, 344) == kGold,
        "full charge shows a pulsing gold ring around the held ball");
  snapCurrent(r, "capture-charge");
  while (int32_t(target - host_time_ms) > 20) { p.next.rightDown = p.next.held = true; frame(r); }
  p.next.rightDown = p.next.held = true;
  frame(r, target - host_time_ms);
  frame(r, 0);
  check(r.engine().view().result == Result::Captured && r.screen().capture.active &&
        r.screen().page == Page::Throw, "perfect release captures and plays the animation in place");
  frame(r, 200); frame(r);
  check(captureFrameAt(r.screen().capture, host_time_ms).stage == CaptureStage::Flight,
        "ball is in flight right after release");
  {
    int x, y;
    captureFlightPos(captureFrameAt(r.screen().capture, host_time_ms).t, x, y);
    bool drawn = false;
    for (int dy = -8; dy <= 8 && !drawn; ++dy) for (int dx = -8; dx <= 8 && !drawn; ++dx)
      drawn = M5.Display.readPixel(x + dx, y + dy) != TFT_BLACK;
    check(drawn, "flying ball is drawn on its arc");
  }
  snapCurrent(r, "capture-flight");
  while (captureFrameAt(r.screen().capture, host_time_ms).stage != CaptureStage::Wiggle) frame(r);
  snapCurrent(r, "capture-wiggle");
  check(M5.Display.readPixel(233, 300) != TFT_BLACK, "caught ball wiggles on the ground");
  while (captureFrameAt(r.screen().capture, host_time_ms).stage != CaptureStage::Reveal) frame(r);
  for (unsigned i = 0; i < 15; ++i) frame(r);
  snapCurrent(r, "capture-reveal");
  unsigned gold = 0;
  for (int y = 60; y < 400; ++y) for (int x = 60; x < 400; ++x)
    if (M5.Display.readPixel(x, y) == kGold) ++gold;
  check(gold > 8, "successful capture bursts gold stars");
  while (r.screen().capture.active) frame(r);
  check(r.screen().page == Page::Result && !r.screen().capture.active,
        "finished animation hands over to the result page");
  check(p.feedbacks >= 8, "animation played throw, landing, wiggle and success haptics");
  frame(r, 500);
  snapCurrent(r, "capture-result-first");
  unsigned halo = 0;
  for (int y = 60; y < 290; ++y) for (int x = 80; x < 390; ++x)
    if (M5.Display.readPixel(x, y) == kGold) ++halo;
  check(halo > 8, "first capture shows the rotating halo and tag on the result page");
  select(r, p, 0);
  check(r.screen().page == Page::Home, "result acknowledgement still returns home");
}

}
int main() {
  c152::begin("ADVENTURE TEST");
  bootAndCapture(); clockAndErrors(); midnightAndSampling(); practiceMidnightAchievement();
  fullRecordRestart(); viewerWakeAndPractice(); firstBallSuccess(); captureAnimation();
  std::printf("%u adventure UI checks, 0 failures\n", checks);
}
