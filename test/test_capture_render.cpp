#include "support/adventure_ui/harness.hpp"
#include "battle_presentation.hpp"

namespace {
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
void winPractice(Runtime& r, TestPlatform& p) {
  for (unsigned i = 0; i < 15 && r.screen().page == Page::Practice; ++i) {
    const BattleView b = r.practiceView();
    if (!b.active) break;
    while (int32_t(b.targetMs - host_time_ms) > 20) frame(r);
    p.next.left = b.expected == Action::Dodge;
    p.next.right = b.expected == Action::Attack;
    p.next.held = true;
    frame(r, b.targetMs - host_time_ms); frame(r);
    if (r.screen().page == Page::Practice && b.active) idle(r, 200);
  }
  const auto result = r.practiceView();
  if (r.screen().page == Page::Practice && !result.active)
    idle(r, result.settledMs + kBattleEffectMs - host_time_ms);
  check(r.screen().page == Page::PracticeThrow, "practice victory reaches the throw page after its final action");
  frame(r);
}
void practicePerfectRelease(Runtime& r, TestPlatform& p) {
  edge(r, p, false, true);
  check(r.screen().practiceThrowing, "practice prepares a throw without consuming a real ball");
  const uint32_t target = r.screen().practiceTargetMs;
  p.next.right = p.next.held = true; frame(r);
  check(r.screen().charging, "practice throwing charges while held");
  while (int32_t(target - host_time_ms) > 20) { p.next.rightDown = p.next.held = true; frame(r); }
  p.next.rightDown = p.next.held = true; frame(r, target - host_time_ms);
  frame(r, 0);
  check(r.screen().capture.active, "practice release starts the capture animation");
}
uint32_t practiceSeed(bool captured, uint16_t exclude = 0) {
  for (uint32_t seed = 1; seed < 4096; ++seed) {
    const uint16_t species = uint16_t(1 + mixRandom(seed ^ 0x9e3779b9u) % kSpeciesCount);
    const CatalogEntry* entry = catalogEntry(species);
    Encounter e; e.species = species; e.seed = seed;
    e.id = (uint64_t(mixRandom(seed)) << 32) | mixRandom(seed ^ 0x85ebca6bu);
    if (entry && !entry->rare && species != exclude &&
        ((throwRoll(e, 1) < 85) == captured)) return seed;
  }
  return 0;
}
void practiceCapture() {
  MemoryStore store; TestPlatform p; Runtime r(store, p); start(r); skipDateAndStarter(r, p);
  const uint32_t successSeed = practiceSeed(true);
  const uint16_t firstSpecies = uint16_t(1 + mixRandom(successSeed ^ 0x9e3779b9u) % kSpeciesCount);
  const uint32_t failSeed = practiceSeed(false, firstSpecies);
  check(successSeed && failSeed, "deterministic practice seeds available");
  p.seed = successSeed;
  openMenu(r, p); select(r, p, 6);
  check(r.screen().page == Page::Practice, "menu offers capture practice");
  check(firstSpecies == r.screen().practiceSpecies,
        "practice rolls a random species for each session");
  const unsigned commits = store.commits;
  const uint64_t steps = r.engine().view().totalSteps;
  winPractice(r, p);
  published(r, "practice-throw-ready");
  practicePerfectRelease(r, p);
  published(r, "practice-throw-flight");
  while (r.screen().capture.active) frame(r);
  frame(r);
  check(r.screen().page == Page::PracticeResult && r.screen().practiceResult == Result::Captured,
        "practice success reaches its own result page");
  check(!r.engine().view().captureCounts[firstSpecies - 1] && r.engine().view().ballsSpent == 0,
        "practice success does not capture or spend a real ball");
  check(r.engine().view().totalSteps == steps && store.commits == commits,
        "practice never commits capture rewards");
  published(r, "practice-result-success");
  p.seed = failSeed;
  select(r, p, 0);
  const uint16_t secondSpecies = r.screen().practiceSpecies;
  check(r.screen().page == Page::Practice && secondSpecies != firstSpecies,
        "practice again rolls a new species");
  winPractice(r, p);
  edge(r, p, false, true);
  idle(r, 1800);
  check(r.screen().practiceUsedBalls == 1 && r.screen().page == Page::PracticeThrow,
        "practice timeout spends one practice ball");
  while (r.screen().capture.active) frame(r);
  frame(r);
  edge(r, p, false, true);
  idle(r, 1800);
  while (r.screen().capture.active) frame(r);
  frame(r);
  check(r.screen().page == Page::PracticeResult && r.screen().practiceResult == Result::Escaped,
        "two practice misses end the session as escaped");
  check(!r.engine().view().captureCounts[secondSpecies - 1],
        "practice failure never touches the real dex");
  select(r, p, 1);
  check(r.screen().page == Page::Home && r.engine().view().phase == Phase::Walking,
        "practice exit keeps the real encounter untouched");
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

}
int main() {
  c152::begin("CAPTURE TEST");
  firstBallSuccess(); practiceCapture(); captureAnimation();
  std::printf("%u capture Runtime/Button_Class/M5GFX checks, 0 failures\n", checks);
}
