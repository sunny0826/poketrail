#include "support/adventure_ui/harness.hpp"
#include <algorithm>

namespace {
void savedVictory(MemoryStore& store, Save& s, bool captured) {
  s.bootGeneration = 1; s.starter = s.partner = 1; setOwned(s, 1); setRegistered(s, 1);
  s.rosterRevision = 1; s.encounterTarget = 300;
  s.totalSteps = s.undatedSteps = 5000; s.explorationSpent = 5000; s.nextEncounterId = 2;
  s.phase = Phase::ThrowReady; s.result = Result::Won;
  s.encounter.id = 1; s.encounter.species = s.encounter.partner = 1; s.encounter.route = Route::Kanto;
  while ((throwRoll(s.encounter, 1, s.encounter.ruleVersion) >=
          captureProbability(s.encounter.ruleVersion, catalogEntry(1)->rare, Timing::Perfect)) == captured)
    ++s.encounter.seed;
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "saved victory fixture validates and encodes");
}
void slowSaveCapture() {
  for (bool captured : {false, true}) {
    MemoryStore store; Save s; savedVictory(store, s, captured);
    TestPlatform p; Runtime r(store, p); start(r);
    store.delayMs = 5000;
    p.next.held = p.next.touch = true; p.next.x = 233; p.next.y = 344; frame(r);
    const uint32_t target = r.engine().throwView().targetMs;
    while (int32_t(target - host_time_ms) > 0) {
      p.next.held = p.next.touchDown = true;
      frame(r, std::min(20u, target - host_time_ms));
    }
    p.next.click = true; p.next.x = 233; p.next.y = 344; frame(r, 0);
    const auto& attempt = r.engine().view().encounter.attempts[0];
    check(attempt.used && attempt.timing == Timing::Perfect && attempt.captured == captured,
          "slow save preserves release-time timing and the committed outcome");
    check(r.screen().page == Page::Throw && r.screen().capture.active &&
          captureFrameAt(r.screen().capture, host_time_ms).stage == CaptureStage::Flight,
          "slow save cannot consume the flight before its first rendered frame");
    snapCurrent(r, captured ? "slow-save-touch-flight-success" : "slow-save-touch-flight-escape");
    while (r.screen().capture.active) frame(r);
    check(r.screen().page == (captured ? Page::Result : Page::Throw),
          "slow-save animation finishes on the page for its saved outcome");
  }
}
}
int main() {
  c152::begin("CAPTURE TEST"); slowSaveCapture();
  std::printf("%u capture Runtime checks, 0 failures\n", checks);
}
