#pragma once
#include "screens.hpp"
#include "motion.hpp"
#include "device_clock.hpp"
#include "clock_sync.hpp"
#include "menu_buttons.hpp"
#include "practice_capture.hpp"
#include "test_mode.hpp"
#include <algorithm>
namespace pokedex { namespace ui {
struct Input {
  bool left = false, right = false, touch = false, click = false, pause = false, back = false;
  bool held = false, leftDown = false, rightDown = false, touchDown = false;
  bool power = false;
  int x = 0, y = 0, swipe = 0;
  uint32_t at = 0;
  bool any() const { return left || right || touch || click || swipe || pause || back; }
};
struct Sample {
  bool fresh = false, ioError = false;
  float x = 0, y = 0, z = 0;
  uint32_t at = 0;
};
enum class Haptic : uint8_t { Light, Medium, Strong, Encounter };
class Platform {
 public:
  virtual ~Platform() {}
  virtual uint32_t now() = 0;
  virtual uint32_t randomSeed() = 0;
  virtual Sample sample() = 0;
  virtual Input pollInput() = 0;
  virtual device_clock::Reading readClock() = 0;
  virtual device_clock::Reading writeClock(int64_t utc) = 0;
  virtual void feedback(uint16_t duration) = 0;
  virtual void feedbackStrength(uint16_t duration, uint8_t strength) {
    (void)strength;
    feedback(duration);
  }
  virtual void feedbackDouble(uint16_t on1, uint16_t gap, uint16_t on2, uint8_t strength) {
    (void)strength;
    feedback(uint16_t(on1 + gap + on2));
  }
  virtual void stopVibration() {}
  virtual bool vibrationActive() = 0;
  virtual bool usbHostPresent() { return false; }
  virtual void brightness(uint8_t) = 0;
  virtual c152::DisplayTransition displaySleep(bool sleeping) = 0;
  virtual c152::BatteryStatus readBattery() = 0;
};
struct Metrics {
  uint32_t maxSampleGapMs = 0, maxInputGapMs = 0;
  uint32_t maxRenderMs = 0, maxCommitMs = 0;
  uint32_t samples = 0, qualityGaps = 0, noSample = 0, imuIoErrors = 0;
  uint32_t rtcErrors = 0, commits = 0, displayErrors = 0;
};
class TimedStore : public adventure::Store {
 public:
  TimedStore(adventure::Store& store, Platform& p, Metrics& m) : store_(store), platform_(p), metrics_(m) {}
  adventure::LoadResult load(adventure::Save& out) override { return store_.load(out); }
  adventure::CommitStatus commit(const adventure::Save& s) override;
 private:
  adventure::Store& store_; Platform& platform_; Metrics& metrics_;
};
class Runtime {
 public:
  enum class TestRequest : uint8_t {
    None, EnterSnapshot, EnterBlank, ResetSnapshot, ResetBlank,
    ScenarioAccount, ScenarioNearEncounter, ScenarioCurrentThrow, ScenarioLegacy, Exit
  };
  Runtime(adventure::Store& store, Platform& platform,
          adventure::TestModeHost* testHost = nullptr);
  bool startStartup();
  bool startupActive() const { return startupActive_; }
  static constexpr uint32_t kStartupDurationMs = 1200;
  bool begin(bool persistBoot = true);
  void loop();
  TestRequest takeTestRequest() { const TestRequest r = testRequest_; testRequest_ = TestRequest::None; return r; }
  void openTestPage(Page, bool failure = false);
  bool canSwitchMode() const { return safe() && !engine_.savePending(); }
  adventure::CommandResult checkpoint() { return engine_.checkpoint(); }
  bool injectSteps(unsigned count);
  const adventure::Engine& engine() const { return engine_; }
  adventure::Engine& engine() { return engine_; }
  const ScreenState& screen() const { return screen_; }
  i18n::Language language() const { return screen_.language; }
  const Metrics& metrics() const { return metrics_; }
  const adventure::BattleView& practiceView() const { return practiceCapture_.battleView(); }
  bool dimmed() const { return display_ != DisplayState::Awake; }
  bool idlePaceAllowed() const {
    return !startupActive_ && !engine_.savePending() &&
           screen_.page != Page::Battle && screen_.page != Page::Practice &&
           screen_.page != Page::Throw && screen_.page != Page::PracticeThrow &&
           screen_.page != Page::Evolution && screen_.page != Page::EvolutionPreview &&
           screen_.page != Page::Viewer;
  }
  // A USB host keeps light sleep from dropping CDC.
  bool pocketIdle() const {
    return display_ == DisplayState::Asleep && !releaseGate_ && !platform_.vibrationActive() &&
           !engine_.savePending() && !platform_.usbHostPresent();
  }
  uint32_t nextSampleMs(uint32_t now) const {
    static constexpr uint32_t kWakeLatencyMarginMs = 40;
    static constexpr uint32_t kMaxSleepWindowMs = motion::kMaxSampleGapMs - kWakeLatencyMarginMs;
    static constexpr uint32_t kSamplePollIntervalMs = 20;
    const uint32_t elapsed = now - lastSample_;
    if (!hasSample_ || elapsed >= kMaxSleepWindowMs) return 0;
    return std::min<uint32_t>(kMaxSleepWindowMs - elapsed, kSamplePollIntervalMs);
  }
  void feedback(uint16_t duration);
  void feedback(Haptic kind);
  clock_sync::Status clockSyncStatus();
  const char* syncClockFromHost(uint32_t id, int64_t utcSeconds, int16_t offsetMinutes);
 private:
  enum class HostClockStage : uint8_t { None, BeginSave, RtcWrite, ConfirmSave };
  clock_sync::Status clockStatus(const device_clock::Reading&) const;
  const char* resumeHostClock();
  bool hostClockVerified(int64_t targetUtc, int16_t offset);
  void clearHostClock();
  void navigate(Page);
  void openMenu(MenuItem focus);
  void syncTestMode();
  void waitForRelease();
  void syncPage();
  void syncLanguage();
  void openLanguage();
  void activateLanguage(unsigned item);
  void leaveLanguage();
  void handle(const Input&);
  bool handleTestHold(const Input&);
  void activate(unsigned item, uint32_t at);
  void activateProgression(unsigned item, uint32_t at);
  void activateTest(unsigned item);
  void handleThrow(const Input&);
  void handlePracticeThrow(const Input&);
  void beginPractice(uint32_t at);
  void endPractice();
  void syncPracticePage();
  void syncPracticeView();
  void startPracticeCapture(const adventure::PracticeOutcome&);
  void tickCapture(uint32_t now);
  void battleFeedback();
  void startEvolution();
  void openEvolutionPreview(uint16_t species);
  void chooseEvolutionPreview(unsigned item);
  void tickEvolution(uint32_t now, bool inputHeld);
  static bool hasEvolutionPreview(uint16_t species, void* context);
  void openPartner(uint16_t species);
  void returnToPartners();
  void openCarriedPartner();
  void openFirstCaptures();
  void jumpFirstCapturesGeneration();
  void recordFirstCapturesSelection();
  void openHomeEvolution();
  void leaveEvolutionConfirm();
  void acknowledgeResult(bool chainNext) {
    if (engine_.acknowledgeResult(engine_.view().encounter.id, platform_.randomSeed()) ==
            adventure::CommandResult::Applied &&
        !engine_.savePending()) {
      navigate(chainNext && engine_.view().phase == adventure::Phase::EncounterReady
                   ? Page::Encounter : Page::Home);
    }
  }
  void activateHomeShortcut(PressedAux aux) {
    if (aux == PressedAux::HomeEvolve) openHomeEvolution();
    else if (aux == PressedAux::HomeRegion) { routesReturn_ = Page::Home; navigate(Page::Routes); }
    else navigate(engine_.view().phase == adventure::Phase::ThrowReady ? Page::Throw : Page::Encounter);
  }
  void resumeSavedProgression();
  static const char* collectionLabel(uint16_t species, void* context);
  void goBack();
  void sample(uint32_t now);
  bool battleFinishing() const;
  bool safe() const;
  void fillTestSteps();
  void pollClock(uint32_t now);
  adventure::ClockReading readingAt(uint32_t at) const;
  adventure::ClockReading makeReading(const device_clock::Reading&, uint32_t at, int offset, bool transaction = false) const;
  void openClock();
  void editClock(int delta);
  void saveClock(uint32_t now);
  void continueClockWrite();
  void discardAnchors();
  void sleepDisplay();
  bool updateDisplayPower(const Input&);
  void pollBattery(uint32_t now, bool force = false);
  Platform& platform_;
  adventure::TestModeHost* testHost_ = nullptr;
  Metrics metrics_;
  TimedStore store_;
  adventure::Engine engine_;
  Screens screens_;
  ScreenState screen_;
  MenuButtons menuButtons_;
  motion::Detector detector_;
  adventure::CapturePractice practiceCapture_;
  adventure::ClockReading anchor_;
  adventure::StepEvent pendingSteps_[131];
  unsigned pendingStepCount_ = 0;
  unsigned testStepsRemaining_ = 0;
  uint32_t lastSamplePoll_ = 0, lastSample_ = 0, lastInput_ = 0;
  uint32_t lastClock_ = 0, lastCheckpoint_ = 0, lastRender_ = 0, lastActivity_ = 0;
  uint32_t lastStepEventMs_ = 0;
  uint64_t batchId_ = 0, notifiedEncounter_ = 0, pendingClockWrite_ = 0;
  uint64_t hostClockId_ = 0;
  int64_t hostClockTargetUtc_ = 0;
  uint32_t hostClockReceivedAt_ = 0;
  int16_t hostClockOffset_ = 0;
  HostClockStage hostClockStage_ = HostClockStage::None;
  bool hostClockWriteAttempted_ = false;
  TestRequest testRequest_ = TestRequest::None;
  Page testEntryReturn_ = Page::Settings;
  Page languageReturn_ = Page::Menu;
  uint16_t languageReturnSelection_ = 0;
  uint32_t testHoldStarted_ = 0;
  uint8_t lastCaptureCue_ = 0;
  uint8_t battleSettlements_ = 0;
  bool testHoldActive_ = false;
  enum class PendingRoster : uint8_t { None, Select, Evolve };
  PendingRoster pendingRoster_ = PendingRoster::None;
  Page evolutionReturn_ = Page::PartnerDetail;
  Page routesReturn_ = Page::Menu;
  bool partnersFromCarried_ = false;
  uint16_t partnersSelectedSpecies_ = 0;
  uint16_t firstCapturesSelected_ = UINT16_MAX;
  bool hasSample_ = false, gapReported_ = false, wasVibrating_ = false, battleEffectChecked_ = false;
  enum class DisplayState : uint8_t { Awake, Asleep, Waking };
  DisplayState display_ = DisplayState::Awake;
  uint32_t wakeStarted_ = 0, wakeDelay_ = 0, lastBattery_ = 0;
  bool clockDeferred_ = true, dirty_ = true, releaseGate_ = false;
  bool dateSkipped_ = false, graphicsReady_ = false;
  uint32_t startupStarted_ = 0, startupRendered_ = 0;
  bool startupActive_ = false;
};
} }
