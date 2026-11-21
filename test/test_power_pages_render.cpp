#include "support/power_fixture.hpp"
#include "support/rendered_text.hpp"

namespace {
class WakeContentPlatform : public PowerPlatform {
 public:
  bool inspectWake = false, sawReadableWake = false;
  void brightness(uint8_t value) override {
    if (inspectWake && value) {
      sawReadableWake = rendered_text::contains("今日 --", theme::kText) &&
                        rendered_text::contains("左 / 右键打开菜单", theme::kMuted);
      check(sawReadableWake, "first illuminated home already shows its status and menu instruction");
      inspectWake = false;
    }
    PowerPlatform::brightness(value);
  }
};
void wake(Runtime& runtime) {
  M5.powerClick(); frame(runtime); idle(runtime, 160); frame(runtime);
  check(!runtime.dimmed(), "power wakes the display");
}
void sleep(Runtime& runtime) { M5.powerClick(); frame(runtime); idle(runtime, 160); }
void autoSleepAndWalk() {
  MemoryStore store; ready(store); WakeContentPlatform platform; Runtime runtime(store, platform); start(runtime);
  idle(runtime, 30020); check(runtime.dimmed(), "inactive home sleeps");
  const auto before = runtime.engine().view().totalSteps;
  walk(runtime, platform, 8); idle(runtime, 30000);
  const auto& save = runtime.engine().view();
  check(save.totalSteps >= before + 7 && save.progress[0].walked == save.totalSteps,
        "sleeping display keeps counting and cultivating");
  Save restored; check(store.load(restored).status == LoadStatus::Loaded && restored.totalSteps == save.totalSteps,
                       "steps earned asleep survive save reload");
  M5.Display.fillScreen(TFT_BLACK);
  platform.inspectWake = true;
  wake(runtime);
  check(platform.sawReadableWake, "wake content was checked at illumination");
  check(runtime.screen().page == Page::Home, "wake does not activate a menu action");
  screenshot(runtime, "power-home");
}
void clockDraft() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, MenuItem::Settings); select(runtime, 0); select(runtime, 0); edge(runtime, false, true);
  const auto draft = runtime.screen(); const unsigned writes = platform.clockWrites;
  idle(runtime, 30020); check(runtime.dimmed() && draft.editing, "clock editor sleeps with an unsaved draft");
  tap(runtime, 233, 390); check(runtime.dimmed(), "touch cannot wake or commit a sleeping clock draft");
  wake(runtime);
  const auto& after = runtime.screen();
  check(after.page == Page::Clock && after.editing && after.selected == draft.selected &&
        after.civil.year == draft.civil.year && after.civil.month == draft.civil.month &&
        after.civil.day == draft.civil.day && after.offset == draft.offset && after.clockMode == draft.clockMode,
        "wake preserves the unsaved clock draft");
  check(platform.clockWrites == writes && !runtime.engine().view().clock.pending.active,
        "sleeping and waking never save a clock draft");
  screenshot(runtime, "power-clock-draft");
}
void pausedAndActive() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, MenuItem::Practice);
  idle(runtime, 6500);
  check(!runtime.dimmed() && runtime.screen().page == Page::Practice, "active practice stays visible");
  M5.powerClick(); frame(runtime);
  check(!runtime.dimmed(), "power cannot interrupt active practice");
  tap(runtime, 233, 30); idle(runtime, 1700);
  check(runtime.screen().page == Page::Pause && runtime.practiceView().paused, "practice reaches pause");
  const auto paused = runtime.practiceView();
  idle(runtime, 30020); check(runtime.dimmed(), "paused practice may sleep");
  wake(runtime);
  check(runtime.screen().page == Page::Pause && runtime.practiceView().paused &&
        runtime.practiceView().cueIndex == paused.cueIndex && runtime.practiceView().playerHp == paused.playerHp,
        "wake preserves the paused battle");
  screenshot(runtime, "power-paused");
}
void failedCommandsAndSave() {
  MemoryStore store; ready(store); PowerPlatform platform; platform.failSleep = true;
  Runtime runtime(store, platform); start(runtime); sleep(runtime);
  check(!runtime.dimmed() && M5.Display.brightness > 0, "rejected sleep leaves a visible UI");
  platform.failSleep = false; sleep(runtime); check(runtime.dimmed(), "power sleeps once the display accepts");
  wake(runtime);
}
void wakeFailureAndSaveRetry() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  sleep(runtime); platform.failWake = true; M5.powerClick(); frame(runtime); idle(runtime, 180);
  check(runtime.dimmed(), "failed wake stays dark");
  platform.failWake = false; wake(runtime);
  sleep(runtime); store.fail = true; walk(runtime, platform, 4); idle(runtime, 30020);
  check(runtime.dimmed() && runtime.engine().savePending(), "sleep retains a failed checkpoint for retry");
  const auto total = runtime.engine().view().totalSteps;
  wake(runtime);
  check(runtime.screen().page == Page::Error && runtime.engine().savePending(), "wake exposes storage failure");
  screenshot(runtime, "power-save-error");
  store.fail = false; select(runtime, 0);
  check(!runtime.engine().savePending() && runtime.engine().view().totalSteps == total,
        "explicit retry preserves the same step result after wake");
}
void viewerErrorWake() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, MenuItem::Pokedex); frame(runtime); idle(runtime, 30020);
  viewer_screen::g_ready = false; wake(runtime);
  check(rendered_text::contains("画面不可用，请重启", theme::kRed),
        "viewer failure remains readable after waking");
  viewer_screen::g_ready = true;
}
class CalibratedPowerPlatform : public PowerPlatform {
 public:
  bool rtcValid = true;
  int64_t utcBase = 1792670400LL;
  uint32_t rtcStartMs = 0;
  device_clock::Reading readClock() override {
    device_clock::Reading reading = {};
    reading.utcSeconds = utcBase + int64_t(now() - rtcStartMs) / 1000;
    reading.valid = rtcValid;
    reading.verificationFailed = !rtcValid;
    return reading;
  }
};
void calibratedAsleepIdleAndWalk() {
  MemoryStore store;
  CalibratedPowerPlatform platform;
  platform.rtcStartMs = host_time_ms;
  {
    Engine engine(store);
    check(engine.begin() == CommandResult::Applied, "calibrated power fixture boots");
    check(engine.chooseStarter(1) == CommandResult::Applied, "calibrated power fixture picks starter");
    ClockReading target;
    target.trusted = true;
    target.utcSeconds = platform.utcBase;
    target.dayKey = 20261022;
    target.bootGeneration = engine.view().bootGeneration;
    target.monotonicMs = host_time_ms;
    check(engine.beginClockChange(1, ClockChangeMode::Normal, target) == CommandResult::Applied &&
          engine.finishClockChange(1, target) == CommandResult::Applied,
          "calibrated power fixture has trusted clock");
  }
  Runtime runtime(store, platform);
  start(runtime);
  check(runtime.engine().view().clock.trusted, "calibrated power runtime starts trusted");
  idle(runtime, 30020);
  check(runtime.dimmed(), "calibrated home sleeps");
  const unsigned commits = store.commits;
  const uint64_t seq = runtime.engine().view().sequence;
  idle(runtime, 65000);
  check(store.commits == commits && runtime.engine().view().sequence == seq,
        "asleep calibrated idle never triggers periodic Flash commits");
  walk(runtime, platform, 4);
  const auto& save = runtime.engine().view();
  check(save.totalSteps >= 3 && save.today.steps == save.totalSteps && save.undatedSteps == 0,
        "asleep steps beyond 2.5s anchor age still attribute to today");
  idle(runtime, 30020);
  check(store.commits == commits + 1, "asleep walking checkpoints once after the 30s idle save interval");
  Save reloaded;
  check(store.load(reloaded).status == LoadStatus::Loaded && reloaded.today.steps == save.today.steps,
        "asleep dated steps survive reload");
  platform.rtcValid = false;
  wake(runtime);
  check(!runtime.engine().view().clock.trusted, "waking immediately refreshes RTC and catches an invalid clock");
}
void activeInputCheckpointDeferral() {
  {
    MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform);
    const uint32_t baseMs = host_time_ms;
    start(runtime);
    walk(runtime, platform, 4);
    const auto steps = runtime.engine().view().totalSteps;
    const unsigned commits = store.commits;
    check(steps >= 3, "active input deferral fixture earns dirty steps");
    openMenu(runtime);
    while (host_time_ms - baseMs < 35000) {
      idle(runtime, 960);
      edge(runtime, false, true);
      check(!runtime.dimmed() && store.commits == commits,
            "active button input past 30s defers periodic checkpoint while awake");
    }
    const uint32_t lastInputAt = host_time_ms - 20;
    idle(runtime, 2800);
    check(!runtime.dimmed() && host_time_ms - lastInputAt < 3000 && store.commits == commits,
          "periodic checkpoint stays deferred inside the 3s post-input window");
    idle(runtime, 300);
    check(!runtime.dimmed() && host_time_ms - lastInputAt >= 3000 && store.commits == commits + 1,
          "periodic checkpoint commits immediately once input stays idle for 3s");
    Save reloaded;
    check(store.load(reloaded).status == LoadStatus::Loaded && reloaded.totalSteps == steps,
          "deferred steps survive save reload after quiet flush");
  }
  {
    MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform);
    const uint32_t baseMs = host_time_ms;
    start(runtime);
    walk(runtime, platform, 4);
    const auto steps = runtime.engine().view().totalSteps;
    const unsigned commits = store.commits;
    openMenu(runtime);
    while (host_time_ms - baseMs < 119000) {
      idle(runtime, 960);
      edge(runtime, false, true);
      check(!runtime.dimmed() && store.commits == commits,
            "continuous active input defers checkpoint before the 120s ceiling");
    }
    while (host_time_ms - baseMs < 121000) {
      idle(runtime, 960);
      edge(runtime, false, true);
    }
    check(!runtime.dimmed() && store.commits == commits + 1,
          "continuous active input still flushes at the 120s kCheckpointDeferralMs ceiling");
    Save reloaded;
    check(store.load(reloaded).status == LoadStatus::Loaded && reloaded.totalSteps == steps,
          "120s ceiling checkpoint persists walked steps");
  }
  {
    MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform);
    const uint32_t baseMs = host_time_ms;
    start(runtime);
    walk(runtime, platform, 4);
    const auto steps = runtime.engine().view().totalSteps;
    const unsigned commits = store.commits;
    openMenu(runtime);
    while (host_time_ms - baseMs < 29000) {
      idle(runtime, 960);
      edge(runtime, false, true);
    }
    edge(runtime, false, true);
    const uint32_t lastInputAt = host_time_ms - 20;
    sleep(runtime);
    check(runtime.dimmed() && store.commits == commits,
          "power key dims the display before the 30s checkpoint window");
    const uint32_t remaining = (baseMs + 30040) > host_time_ms ? (baseMs + 30040) - host_time_ms : 20;
    idle(runtime, remaining);
    check(runtime.dimmed() && host_time_ms - lastInputAt < 3000 && store.commits == commits + 1,
          "dimmed display bypasses active-input deferral when 30s checkpoint is due");
    Save reloaded;
    check(store.load(reloaded).status == LoadStatus::Loaded && reloaded.totalSteps == steps,
          "dimmed checkpoint after recent input survives save reload");
  }
}
void wraparound() {
  host_time_ms = UINT32_MAX - 30050;
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  idle(runtime, 30020); check(runtime.dimmed(), "idle sleep survives clock rollover"); wake(runtime);
}
struct HapticCall {
  enum class Kind : uint8_t { Single, Double };
  Kind kind;
  uint16_t on1, gap, on2;
  uint8_t strength;
  uint32_t at;
};
class HapticPowerPlatform : public PowerPlatform {
 public:
  std::vector<HapticCall> calls;
  unsigned stopCalls = 0;
  bool forceInactive = false;
  void emit(HapticCall::Kind kind, uint16_t on1, uint16_t gap, uint16_t on2, uint8_t strength) {
    const uint16_t total = uint16_t(on1 + gap + on2);
    ++feedbacks;
    lastFeedback = total;
    vibrationEnd = forceInactive ? now() : now() + total;
    calls.push_back({kind, on1, gap, on2, strength, now()});
  }
  void feedback(uint16_t ms) override { emit(HapticCall::Kind::Single, ms, 0, 0, 0); }
  void feedbackStrength(uint16_t ms, uint8_t strength) override {
    emit(HapticCall::Kind::Single, ms, 0, 0, strength);
  }
  void feedbackDouble(uint16_t on1, uint16_t gap, uint16_t on2, uint8_t strength) override {
    emit(HapticCall::Kind::Double, on1, gap, on2, strength);
  }
  void stopVibration() override { ++stopCalls; vibrationEnd = now(); }
};
class SlowCommitStore : public MemoryStore {
 public:
  uint32_t delayMs = 0;
  HapticPowerPlatform* observed = nullptr;
  unsigned stopsBefore = 0;
  bool stoppedBeforeDelay = false;
  CommitStatus commit(const Save& s) override {
    if (observed) stoppedBeforeDelay = observed->stopCalls > stopsBefore && !observed->vibrationActive();
    host_time_ms += delayMs;
    return MemoryStore::commit(s);
  }
};
void checkBoardVib(uint32_t dt, bool active, uint8_t vib, const char* msg) {
  host_time_ms += dt;
  c152::updateVibration();
  check(c152::vibrationActive() == active && M5.Power.vibration == vib, msg);
}
void boardPulseStateMachine() {
  host_time_ms = 10000;
  c152::stopVibration();
  M5.Power.vibration = 0;
  check(!c152::vibrationActive(), "board vibration starts idle");
  c152::feedbackPulse(65, 50, 85, 255);
  checkBoardVib(0, true, 255, "pulse 1 starts at 255");
  checkBoardVib(60, true, 255, "pulse 1 holds before 65ms");
  checkBoardVib(5, true, 0, "pulse 1 enters gap keeping active true");
  checkBoardVib(45, true, 0, "gap holds off before 50ms");
  checkBoardVib(5, true, 255, "pulse 2 starts at 255");
  checkBoardVib(80, true, 255, "pulse 2 holds before 85ms");
  checkBoardVib(5, false, 0, "pulse 2 completes idle");

  c152::feedbackPulse(65, 50, 85, 255);
  checkBoardVib(65, true, 0, "gap entered before stop");
  c152::stopVibration();
  check(!c152::vibrationActive() && M5.Power.vibration == 0, "stopVibration cancels gap pulse 2");
  checkBoardVib(100, false, 0, "cancelled double pulse stays off");

  c152::feedback(0, 80, 215);
  check(c152::vibrationActive() && M5.Power.vibration == 215, "single pulse sets strength 215");
  c152::stopVibration();
  check(!c152::vibrationActive() && M5.Power.vibration == 0, "stopVibration clears single pulse");
}
void feedStrongCrest(Runtime& runtime, HapticPowerPlatform& platform) {
  for (float z : {0.70f, 0.70f, 1.30f, 1.30f, 0.70f}) { platform.z = z; frame(runtime); }
  platform.z = 1.0f;
}
void encounterDoublePulseAndSuppression() {
  host_time_ms = 20000;
  MemoryStore store;
  {
    Engine engine(store);
    check(engine.begin() == CommandResult::Applied && engine.chooseStarter(1) == CommandResult::Applied,
          "encounter haptic fixture boots");
    step(engine, 1, engine.view().encounterTarget - 1);
    check(engine.checkpoint() == CommandResult::Applied, "encounter haptic fixture checkpoints");
  }
  HapticPowerPlatform platform; platform.forceInactive = true;
  Runtime runtime(store, platform); start(runtime); platform.calls.clear();
  for (unsigned ms = 0; ms < 3000 && runtime.engine().view().phase != Phase::EncounterReady; ms += 20) {
    platform.z = 1 - 0.30f * std::cos(6.28318530718f * float(ms % 500) / 500.0f);
    frame(runtime);
  }
  platform.z = 1;
  check(runtime.engine().view().phase == Phase::EncounterReady, "walking triggers encounter");
  check(platform.calls.size() == 1 && platform.calls[0].kind == HapticCall::Kind::Double &&
            platform.calls[0].on1 == 65 && platform.calls[0].gap == 50 &&
            platform.calls[0].on2 == 85 && platform.calls[0].strength == 255,
        "new encounter triggers 65/50/85ms double pulse at strength 255");

  const uint64_t stepsAt = runtime.engine().view().totalSteps;
  const uint32_t encAt = platform.calls[0].at;
  idle(runtime, 220); feedStrongCrest(runtime, platform);
  check(host_time_ms - encAt < 400, "first crest completes inside 400ms suppression window");
  idle(runtime, 160); feedStrongCrest(runtime, platform);
  check(runtime.engine().view().totalSteps == stepsAt, "encounter double pulse suppresses 200+200ms");
  idle(runtime, 140); feedStrongCrest(runtime, platform);
  check(runtime.engine().view().totalSteps > stepsAt, "walking resumes after 400ms window");
}
void slowCommitStopsVibration() {
  host_time_ms = 40000;
  SlowCommitStore store; ready(store, 2);
  HapticPowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  walk(runtime, platform, 3);
  store.observed = &platform; store.delayMs = 800; store.stopsBefore = platform.stopCalls;
  runtime.feedback(Haptic::Strong);
  check(platform.vibrationActive(), "vibration active before slow commit");
  const uint32_t beforeMs = host_time_ms;
  check(runtime.checkpoint() == CommandResult::Applied, "slow commit succeeds");
  check(store.stoppedBeforeDelay && host_time_ms - beforeMs >= 800,
        "TimedStore::commit stops vibration before 800ms store commit");
}
void battleHapticTiersAndWalkingMute() {
  host_time_ms = 60000;
  MemoryStore store; ready(store);
  HapticPowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  idle(runtime, 2100); openMenu(runtime); select(runtime, MenuItem::Practice);
  platform.calls.clear(); walk(runtime, platform, 6);
  check(runtime.practiceView().settlements > 0 && platform.calls.empty(),
        "battle beats within 2s of walking suppress vibration");
  tap(runtime, 233, 30); idle(runtime, 1000);
  check(runtime.screen().page == Page::Pause, "first practice pauses");
  select(runtime, 1); idle(runtime, 2200);

  openMenu(runtime); select(runtime, MenuItem::Practice);
  auto judgeBeat = [&](int32_t leadMs, bool press, Timing timing, uint16_t ms, uint8_t str, const char* msg) {
    while (runtime.practiceView().consumed) frame(runtime);
    if (press) {
      while (int32_t(runtime.practiceView().targetMs - host_time_ms) > leadMs) frame(runtime);
      platform.calls.clear();
      const Action act = runtime.practiceView().expected;
      edge(runtime, act == Action::Dodge, act == Action::Attack);
    } else {
      const uint8_t s = runtime.practiceView().settlements;
      platform.calls.clear();
      while (runtime.practiceView().settlements == s) frame(runtime);
    }
    check(runtime.practiceView().lastTiming == timing && platform.calls.size() == 1 &&
              platform.calls[0].kind == HapticCall::Kind::Single &&
              platform.calls[0].on1 == ms && platform.calls[0].strength == str, msg);
  };
  judgeBeat(20, true, Timing::Perfect, 80, 255, "perfect hit triggers Strong 80ms/255");
  judgeBeat(130, true, Timing::Normal, 50, 215, "normal hit triggers Medium 50ms/215");
  judgeBeat(0, false, Timing::Miss, 35, 180, "missed beat triggers Light 35ms/180");
}
}
int main() {
  c152::begin("POWER PAGES TEST");
  autoSleepAndWalk(); clockDraft(); pausedAndActive(); failedCommandsAndSave(); wakeFailureAndSaveRetry(); viewerErrorWake(); calibratedAsleepIdleAndWalk(); activeInputCheckpointDeferral(); wraparound();
  boardPulseStateMachine(); encounterDoublePulseAndSuppression(); slowCommitStopsVibration(); battleHapticTiersAndWalkingMute();
  std::printf("%u power Runtime checks, 0 failures\n", checks);
}
