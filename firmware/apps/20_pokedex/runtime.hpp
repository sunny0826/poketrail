#pragma once
#include "screens.hpp"
#include "motion.hpp"
#include "device_clock.hpp"
#include "menu_buttons.hpp"
#include "practice_capture.hpp"
#include <algorithm>
namespace pokedex { namespace ui {
struct Input {
  bool left = false, right = false, touch = false, click = false, pause = false, back = false;
  bool held = false, leftDown = false, rightDown = false;
  int x = 0, y = 0, swipe = 0;
  uint32_t at = 0;
  bool any() const { return left || right || touch || click || swipe || pause || back; }
};
struct Sample {
  bool fresh = false, ioError = false;
  float x = 0, y = 0, z = 0;
  uint32_t at = 0;
};
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
  Runtime(adventure::Store& store, Platform& platform);
  bool begin();
  void loop();
  const adventure::Engine& engine() const { return engine_; }
  const ScreenState& screen() const { return screen_; }
  const Metrics& metrics() const { return metrics_; }
  const adventure::BattleView& practiceView() const { return practiceCapture_.battleView(); }
  bool dimmed() const { return display_ != DisplayState::Awake; }
  // Pocket idle may pause the CPU until the next sample. Vibration, an
  // unfinished save, and an attached USB host keep the loop awake; a key only
  // ends the current pause. A USB host keeps light sleep from dropping CDC.
  bool pocketIdle() const {
    return display_ == DisplayState::Asleep && !releaseGate_ && !platform_.vibrationActive() &&
           !engine_.savePending() && !platform_.usbHostPresent();
  }
  uint32_t nextSampleMs(uint32_t now) const {
    // A 100ms gap discards the walking cadence. Keep 40ms for wake latency.
    const uint32_t elapsed = now - lastSample_;
    if (!hasSample_ || elapsed >= 60) return 0;
    return std::min<uint32_t>(60 - elapsed, 20);
  }
  void feedback(uint16_t duration);
 private:
  void navigate(Page);
  void waitForRelease();
  void syncPage();
  void handle(const Input&);
  void activate(unsigned item, uint32_t at);
  void activateProgression(unsigned item, uint32_t at);
  void handleThrow(const Input&);
  void handlePracticeThrow(const Input&);
  void beginPractice(uint32_t at);
  void endPractice();
  void syncPracticePage();
  void syncPracticeView();
  void startPracticeCapture(const adventure::PracticeOutcome&);
  void tickCapture(uint32_t now);
  void startEvolution();
  void openEvolutionPreview(uint16_t species);
  void chooseEvolutionPreview(unsigned item);
  void tickEvolution(uint32_t now, bool inputHeld);
  static bool hasEvolutionPreview(uint16_t species, void* context);
  void openPartner(uint16_t species);
  void resumeSavedProgression();
  static const char* collectionLabel(uint16_t species, void* context);
  void goBack();
  void sample(uint32_t now);
  bool safe() const;
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
  uint32_t lastSamplePoll_ = 0, lastSample_ = 0, lastInput_ = 0;
  uint32_t lastClock_ = 0, lastCheckpoint_ = 0, lastRender_ = 0, lastActivity_ = 0;
  uint64_t batchId_ = 0, notifiedEncounter_ = 0, pendingClockWrite_ = 0;
  uint64_t deferredThrow_ = 0;
  uint8_t lastCaptureCue_ = 0;
  enum class PendingRoster : uint8_t { None, Select, Evolve };
  PendingRoster pendingRoster_ = PendingRoster::None;
  bool hasSample_ = false, gapReported_ = false, wasVibrating_ = false;
  enum class DisplayState : uint8_t { Awake, Asleep, Waking };
  DisplayState display_ = DisplayState::Awake;
  uint32_t wakeStarted_ = 0, wakeDelay_ = 0, lastBattery_ = 0;
  bool clockDeferred_ = true, dirty_ = true, releaseGate_ = false;
  bool dateSkipped_ = false, graphicsReady_ = false;
};
} }
