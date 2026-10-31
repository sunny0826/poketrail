#pragma once

namespace {
void savedVictory(MemoryStore& store, Save& s, bool captured) {
  s.bootGeneration = 1; s.starter = s.partner = 1; setOwned(s, 1); setRegistered(s, 1);
  s.rosterRevision = 1; s.encounterTarget = 300;
  s.totalSteps = 5000; s.undatedSteps = 4500; s.explorationSpent = 5000; s.nextEncounterId = 2;
  s.clock.calibrated = true; s.clock.offsetMinutes = 480; s.clock.dayHighWater = 20261022;
  s.today.dayKey = 20261022; s.today.steps = s.bestDaySteps = 500;
  s.phase = Phase::ThrowReady; s.result = Result::Won;
  s.encounter.id = 1; s.encounter.species = s.encounter.partner = 1;
  while ((throwRoll(s.encounter, 1, s.encounter.ruleVersion) >=
          captureProbability(s.encounter.ruleVersion, catalogEntry(1)->rare, Timing::Perfect)) == captured)
    ++s.encounter.seed;
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "saved victory fixture validates and encodes");
}
void firstBallSuccess() {
  MemoryStore store; Save s;
  savedVictory(store, s, true);
  TestPlatform p; Runtime r(store, p); start(r);
  check(r.screen().page == Page::Throw && !r.engine().throwView().active, "saved victory restarts at explicit preparation");
  throwAt(r, p);
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
  const uint32_t heldAt = host_time_ms;
  p.next.right = p.next.held = true; frame(r);
  check(r.engine().throwView().active && r.screen().charging,
        "the first hold opens the throw window and charges the ball");
  const uint32_t target = r.engine().throwView().targetMs;
  check(int32_t(target - (heldAt + kThrowChargeMs)) >= 0 &&
        int32_t(target - (heldAt + kThrowChargeMs)) < 40,
        "the window targets one charge span after the hold starts");
  for (unsigned held = 0; held < 920; held += 20) { p.next.rightDown = p.next.held = true; frame(r); }
  p.next.rightDown = p.next.held = true; frame(r);
  check(M5.Display.readPixel(267, 344) == theme::kGold || M5.Display.readPixel(263, 344) == theme::kGold,
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
  while (captureFrameAt(r.screen().capture, host_time_ms).stage != CaptureStage::Flash) frame(r);
  frame(r, 60); snapCurrent(r, "capture-flash");
  while (captureFrameAt(r.screen().capture, host_time_ms).stage != CaptureStage::Drop) frame(r);
  frame(r, 200); snapCurrent(r, "capture-drop");
  while (captureFrameAt(r.screen().capture, host_time_ms).stage != CaptureStage::Wiggle) frame(r);
  snapCurrent(r, "capture-wiggle");
  check(M5.Display.readPixel(233, 300) != TFT_BLACK, "caught ball wiggles on the ground");
  while (captureFrameAt(r.screen().capture, host_time_ms).stage != CaptureStage::Reveal) frame(r);
  for (unsigned i = 0; i < 15; ++i) frame(r);
  snapCurrent(r, "capture-reveal");
  unsigned gold = 0;
  for (int y = 60; y < 400; ++y) for (int x = 60; x < 400; ++x)
    if (M5.Display.readPixel(x, y) == theme::kGold) ++gold;
  check(gold > 8, "successful capture bursts gold stars");
  while (r.screen().capture.active) frame(r);
  check(r.screen().page == Page::Result && !r.screen().capture.active,
        "finished animation hands over to the result page");
  check(p.feedbacks >= 8, "animation played throw, landing, wiggle and success haptics");
  frame(r, 500);
  snapCurrent(r, "capture-result-first");
  unsigned halo = 0;
  for (int y = 60; y < 290; ++y) for (int x = 80; x < 390; ++x)
    if (M5.Display.readPixel(x, y) == theme::kGold) ++halo;
  check(halo > 8, "first capture shows the rotating halo and tag on the result page");
  select(r, p, 0);
  check(r.screen().page == Page::Home, "result acknowledgement still returns home");
}
void captureEscape() {
  MemoryStore store; Save s; savedVictory(store, s, false);
  TestPlatform p; Runtime r(store, p); start(r);
  throwAt(r, p);
  check(r.screen().capture.active && !r.screen().capture.captured &&
        r.engine().view().encounter.usedBalls == 1 && !r.engine().view().recentSize,
        "failed throw commits one ball without awarding a capture before escape");
  for (unsigned i = 0; i < 200 && captureFrameAt(r.screen().capture, host_time_ms).stage != CaptureStage::Reveal; ++i) frame(r);
  check(captureFrameAt(r.screen().capture, host_time_ms).stage == CaptureStage::Reveal,
        "failed throw reaches its saved roll's escape frame");
  frame(r, 120); snapCurrent(r, "capture-escape");
  const uint32_t end = r.screen().capture.startMs + captureTotalMs(r.screen().capture);
  frame(r, end - 1 - host_time_ms);
  check(r.screen().capture.active && captureFrameAt(r.screen().capture, host_time_ms).stage == CaptureStage::Reveal,
        "escape remains visible until its result hold ends");
  frame(r, 1);
  check(r.screen().page == Page::Throw && !r.screen().capture.active && !r.engine().throwView().active,
        "escape returns to explicit preparation for the second ball");
  frame(r); p.next.right = p.next.held = true; frame(r);
  check(r.engine().throwView().active && r.engine().view().encounter.usedBalls == 1 && !r.engine().view().recentSize,
        "fresh input prepares second ball without spending it or adding a capture");
}
}
