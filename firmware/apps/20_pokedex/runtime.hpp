#pragma once
#include "screens.hpp"
#include "motion.hpp"
#include "device_clock.hpp"
#include "menu_buttons.hpp"
namespace pokedex { namespace ui {
struct Input {
  bool left = false, right = false, touch = false, pause = false, back = false;
  bool held = false, leftDown = false, rightDown = false;
  int x = 0, y = 0;
  uint32_t at = 0;
  bool any() const { return left || right || touch || pause || back; }
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
  const adventure::BattleView& practiceView() const { return practice_.view(); }
  bool dimmed() const { return display_ != DisplayState::Awake; }
  void feedback(uint16_t duration);
 private:
  void navigate(Page);
  void waitForRelease();
  void syncPage();
  void handle(const Input&);
  void activate(unsigned item, uint32_t at);
  void activateProgression(unsigned item, uint32_t at);
  void handleThrow(const Input&);
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
  adventure::Battle practice_;
  adventure::ClockReading anchor_;
  adventure::StepEvent pendingSteps_[131];
  unsigned pendingStepCount_ = 0;
  uint32_t lastSamplePoll_ = 0, lastSample_ = 0, lastInput_ = 0;
  uint32_t lastClock_ = 0, lastCheckpoint_ = 0, lastRender_ = 0, lastActivity_ = 0;
  uint64_t batchId_ = 0, notifiedEncounter_ = 0, pendingClockWrite_ = 0;
  uint64_t deferredThrow_ = 0;
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
