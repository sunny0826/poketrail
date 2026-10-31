#include "runtime.hpp"
#include "viewer_screen.hpp"
#include "battle_presentation.hpp"
#include <algorithm>
namespace pokedex { namespace ui {
using namespace adventure;
namespace {
// A checkpoint write blocks sampling long enough to break walking cadence.
// Defer it while steps keep arriving and release once the cadence window
// (kMaxStepMs) has lapsed, or force it after the deferral cap so progress
// still reaches flash during long uninterrupted walks.
constexpr uint32_t kStepActiveMs = 2000;
constexpr uint32_t kCheckpointDeferralMs = 120000;
}
CommitStatus TimedStore::commit(const Save& s) {
  uint32_t start = platform_.now();
  auto result = store_.commit(s);
  metrics_.maxCommitMs = std::max(metrics_.maxCommitMs, platform_.now() - start);
  ++metrics_.commits; return result;
}
Runtime::Runtime(Store& store, Platform& platform, TestModeHost* testHost)
    : platform_(platform), testHost_(testHost), store_(store, platform, metrics_), engine_(store_) {}
bool Runtime::begin(bool persistBoot) {
  graphicsReady_ = screens_.begin();
  viewer_screen::setFeedback([](uint16_t d, void* context) { static_cast<Runtime*>(context)->feedback(d); }, this);
  engine_.begin(persistBoot);
  uint32_t now = platform_.now();
  if (!persistBoot && engine_.ready()) {
    const auto reading = makeReading(platform_.readClock(), now,
                                     engine_.view().clock.offsetMinutes, false);
    if (reading.trusted) engine_.adoptClock(reading);
  }
  syncTestMode();
  lastActivity_ = lastSamplePoll_ = lastInput_ = lastCheckpoint_ = now;
  pollBattery(now, true);
  batchId_ = engine_.view().lastStepBatch;
  notifiedEncounter_ = engine_.view().encounter.id;
  syncPage();
  if (engine_.ready()) pollClock(now);
  return graphicsReady_;
}
bool Runtime::injectSteps(unsigned count) {
  if (!testHost_ || !testHost_->testMode() || !engine_.ready() || engine_.savePending()) return false;
  if (count > UINT32_MAX - testStepsRemaining_) return false;
  testStepsRemaining_ += count;
  fillTestSteps();
  return true;
}
void Runtime::openTestPage(Page page, bool failure) {
  navigate(page);
  if (page == Page::TestEntry && screen_.testMode) testEntryReturn_ = Page::TestHub;
  screen_.testFailure = failure;
  dirty_ = true;
}
void Runtime::fillTestSteps() {
  if (!testStepsRemaining_ || engine_.savePending()) return;
  if (!testHost_ || !testHost_->testMode() || !engine_.ready()) {
    testStepsRemaining_ = 0;
    return;
  }
  // Keep one injection batch within the Engine step queue (128) so a failed
  // commit can retain the tail instead of overflowing it.
  const unsigned capacity = 128;
  const unsigned queued = unsigned(engine_.bufferedSteps());
  if (queued >= capacity) return;
  const unsigned room = capacity - queued;
  const uint32_t now = platform_.now();
  while (testStepsRemaining_ && pendingStepCount_ < capacity && pendingStepCount_ < room) {
    StepEvent event;
    event.batchId = ++batchId_;
    event.clock = readingAt(now);
    event.partner = engine_.view().partner;
    pendingSteps_[pendingStepCount_++] = event;
    --testStepsRemaining_;
  }
}
void Runtime::syncTestMode() {
  if (!testHost_) return;
  const bool wasTest = screen_.testMode;
  screen_.testMode = testHost_->testMode();
  if (screen_.testMode) {
    const auto& clock = engine_.view().clock;
    if (clock.calibrated && !clock.pending.active && clock.anchor.utcSeconds)
      testHost_->mode().beginSimulation(platform_.now(), clock.anchor.utcSeconds, clock.offsetMinutes);
  }
  if (screen_.testMode == wasTest) return;
  screens_.invalidate();
  dirty_ = true;
}

void Runtime::waitForRelease() {
  menuButtons_.reset(); releaseGate_ = true;
}
void Runtime::navigate(Page page) {
  screen_.page = page; screen_.selected = 0; screen_.editing = false;
  screen_.testFailure = false;
  screens_.invalidate(); dirty_ = true; waitForRelease();
  screen_.command = CommandResult::Applied;
  if (page == Page::Viewer) {
    viewer_screen::setCollectionLabel(&Runtime::collectionLabel, this);
    viewer_screen::setEvolutionPreview(&Runtime::hasEvolutionPreview, this);
    viewer_screen::invalidate(); viewer_screen::begin();
  }
}
bool Runtime::safe() const {
  const PracticeStage stage = practiceCapture_.stage();
  const bool practiceBusy = (stage == PracticeStage::Battle && !practiceCapture_.battleView().paused) ||
                            stage == PracticeStage::Throwing;
  return !(engine_.battleView().active && !engine_.battleView().paused) && !engine_.throwView().active &&
         !practiceBusy && screen_.page != Page::Evolution && screen_.page != Page::EvolutionPreview;
}
void Runtime::syncPage() {
  if (screen_.testMode && testHost_ &&
      (screen_.page == Page::TestHub || screen_.page == Page::TestFaults ||
       screen_.page == Page::TestDiagnostics)) {
    screen_.testFault = testHost_->fault();
    screen_.testDiagnostics = testHost_->diagnostics();
  }
  const auto& v = engine_.view();
  if (engine_.ready() && !engine_.savePending() && v.phase == Phase::EncounterReady && v.encounter.id != notifiedEncounter_) {
    notifiedEncounter_ = v.encounter.id; feedback(35);
  }
  if (isTestPage(screen_.page)) return;
  if (screen_.page == Page::Viewer || screen_.page == Page::EvolutionPreviewTargets ||
      screen_.page == Page::EvolutionPreview) return;
  if (screen_.page == Page::Throw && screen_.capture.active) return;
  if (engine_.savePending() || !engine_.ready()) { if (screen_.page != Page::Error) navigate(Page::Error); return; }
  if ((!v.starter && !dateSkipped_ && !v.clock.calibrated) || (v.clock.pending.active && !pendingClockWrite_ && !dateSkipped_)) {
    if (screen_.page != Page::Clock && screen_.page != Page::DateWelcome) navigate(Page::DateWelcome);
    return;
  }
  if (!v.starter && screen_.page != Page::Clock && screen_.page != Page::DateWelcome) {
    if (screen_.page != Page::Starter) navigate(Page::Starter); return;
  }
  if (screen_.page == Page::Practice || screen_.page == Page::PracticeThrow ||
      screen_.page == Page::PracticeResult || (screen_.page == Page::Pause && screen_.practice)) return;
  if (screen_.page == Page::Battle && !engine_.battleView().active &&
      battleEffectActive(engine_.battleView(), platform_.now())) return;
  if (v.phase == Phase::ThrowReady) {
    if (screen_.page != Page::Throw) navigate(Page::Throw);
  } else if (v.phase == Phase::Resolved) {
    if (screen_.page != Page::Result) navigate(Page::Result);
  }
  else if (v.phase == Phase::Battle) {
    Page page = engine_.battleView().paused ? Page::Pause : Page::Battle;
    if (screen_.page != page) navigate(page);
  } else if (screen_.page == Page::Error || screen_.page == Page::Battle || screen_.page == Page::Throw || screen_.page == Page::Result) navigate(Page::Home);
}
void Runtime::feedback(uint16_t duration) {
  platform_.feedback(duration);
  detector_.suppressUntil(platform_.now() + duration + 200);
  wasVibrating_ = true;
}
void Runtime::discardAnchors() {
  detector_.reset(); anchor_ = ClockReading(); clockDeferred_ = true;
}
void Runtime::sample(uint32_t now) {
  if (platform_.vibrationActive()) {
    detector_.suppressUntil(now + 200); wasVibrating_ = true;
  } else if (wasVibrating_) {
    detector_.suppressUntil(now + 200); wasVibrating_ = false;
  }
  if (hasSample_) metrics_.maxSampleGapMs = std::max(metrics_.maxSampleGapMs, now - lastSample_);
  if (hasSample_ && now - lastSample_ >= 100 && !gapReported_) {
    ++metrics_.qualityGaps; gapReported_ = true; discardAnchors();
  }
  if (now - lastSamplePoll_ < 20) return;
  lastSamplePoll_ = now;
  const Sample s = platform_.sample();
  if (s.ioError) ++metrics_.imuIoErrors;
  if (!s.fresh) { ++metrics_.noSample; return; }
  if (hasSample_) metrics_.maxSampleGapMs = std::max(metrics_.maxSampleGapMs, s.at - lastSample_);
  hasSample_ = true; lastSample_ = s.at; gapReported_ = false; ++metrics_.samples;
  const auto batch = detector_.sample(s.x, s.y, s.z, s.at, engine_.ready() ? engine_.view().partner : 0);
  if (batch.qualityGap) { ++metrics_.qualityGaps; discardAnchors(); }
  if (batch.count) lastStepEventMs_ = now;
  for (unsigned i = 0; i < batch.count; ++i) {
    if (!engine_.ready() || batchId_ == UINT64_MAX) continue;
    StepEvent event; event.batchId = ++batchId_; event.clock = readingAt(batch.events[i].monotonicMs);
    event.partner = batch.events[i].tag;
    pendingSteps_[pendingStepCount_++] = event;
    if (pendingStepCount_ >= 128 && practiceCapture_.active()) endPractice();
  }
}
void Runtime::loop() {
  const uint32_t start = platform_.now();
  sample(start);
  Input input = platform_.pollInput();
  metrics_.maxInputGapMs = std::max(metrics_.maxInputGapMs, input.at - lastInput_); lastInput_ = input.at;
  if (input.any() || input.held) lastActivity_ = input.at;
  if (pendingStepCount_ && !(practiceCapture_.stage() == PracticeStage::Battle &&
                             !practiceCapture_.battleView().paused)) {
    engine_.observeSteps(pendingSteps_, pendingStepCount_, platform_.randomSeed());
    pendingStepCount_ = 0; dirty_ = true;
  }
  if (!pendingStepCount_ && !engine_.savePending() && engine_.bufferedSteps() && safe())
    engine_.flushBufferedSteps();
  fillTestSteps();
  const bool waking = updateDisplayPower(input);
  if (input.power && !waking && !dimmed() && safe()) sleepDisplay();
  if (engine_.ready() &&
      (start - lastClock_ >= 1000 || (clockDeferred_ && safe() && !engine_.savePending()))) pollClock(platform_.now());
  const bool blockedInput = releaseGate_;
  const bool viewerWasActive = screen_.page == Page::Viewer;
  if (releaseGate_) {
    if (!input.held && (!input.any() || input.click || input.swipe)) releaseGate_ = false;
  } else if (!dimmed()) handle(input);
  if (releaseGate_ && !input.held && (input.click || input.swipe)) releaseGate_ = false;
  if (practiceCapture_.stage() != PracticeStage::Idle) {
    const auto outcome = practiceCapture_.tick(platform_.now());
    if (outcome.valid) startPracticeCapture(outcome);
  }
  syncPracticeView();
  syncPracticePage();
  if (engine_.ready() && !engine_.savePending()) engine_.tick(platform_.now(), readingAt(platform_.now()));
  tickCapture(platform_.now());
  tickEvolution(platform_.now(), input.held);
  syncPage();
  if (safe() && !engine_.savePending() && clockDeferred_ && engine_.ready()) pollClock(platform_.now());
  const bool timedPage = screen_.page == Page::Battle || screen_.page == Page::Practice ||
                         screen_.page == Page::Throw || screen_.page == Page::PracticeThrow ||
                         screen_.page == Page::Evolution || screen_.page == Page::EvolutionPreview;
  if (!dimmed() && safe() && platform_.now() - lastActivity_ >= 30000) sleepDisplay();
  if (!dimmed()) pollBattery(platform_.now());
  const uint32_t now = platform_.now();
  const bool sceneActive = !dimmed() && graphicsReady_ && screen_.page == Page::Home;
  dirty_ |= screens_.advanceScene(engine_.view().route, now, sceneActive);
  if (!dimmed() && graphicsReady_) {
    if (screen_.page == Page::Home) dirty_ |= screens_.advanceHome(engine_.view().partner, now);
    if (screen_.page == Page::Result && engine_.view().result == Result::Captured)
      dirty_ |= screens_.advanceResult(engine_.view().encounter.species, now);
    if (screen_.page == Page::PracticeResult && practiceCapture_.result() == Result::Captured)
      dirty_ |= screens_.advanceResult(practiceCapture_.species(), now);
    if (screen_.page == Page::Viewer && waking) {
      if (!viewer_screen::redraw(now)) { drawGraphicsError(screen_.battery); ++metrics_.displayErrors; }
      if (screen_.testMode) viewer_screen::paintTestBadge();
    } else if (screen_.page == Page::Viewer && viewerWasActive && !releaseGate_ && !blockedInput) {
      viewer_screen::update(now);
      if (screen_.testMode) viewer_screen::paintTestBadge();
    } else if (screen_.page != Page::Viewer && (dirty_ || now - lastRender_ >= (timedPage ? 40u : 500u))) {
      if (!screens_.draw(screen_, engine_, practiceCapture_.battleView(), now)) ++metrics_.displayErrors;
      lastRender_ = now; dirty_ = false;
    }
    metrics_.maxRenderMs = std::max(metrics_.maxRenderMs, platform_.now() - now);
  }
  if (waking) {
    if (!graphicsReady_) { drawGraphicsError(screen_.battery); ++metrics_.displayErrors; }
    platform_.brightness(96);
  }
  if (engine_.ready() && safe() && !engine_.savePending() && platform_.now() - lastCheckpoint_ >= 30000) {
    const uint32_t due = platform_.now() - lastCheckpoint_;
    const bool stepping = platform_.now() - lastStepEventMs_ < kStepActiveMs;
    if (!stepping || due >= kCheckpointDeferralMs) {
      engine_.checkpoint(); lastCheckpoint_ = platform_.now();
    }
  }
}
} }
