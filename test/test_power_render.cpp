#include "support/progression_ui.hpp"
#include "sprites_generated.hpp"
using namespace progression_ui;
namespace pokedex { namespace viewer_screen { extern viewer::State g_state; extern bool g_ready; } }
namespace {
class PowerPlatform : public HostPlatform {
 public:
  c152::BatteryStatus battery;
  unsigned reads = 0, sleeps = 0, wakes = 0, feedbacks = 0, clockWrites = 0;
  bool failSleep = false, failWake = false, observeWake = false, usb = false;
  std::vector<uint16_t> litFrame;
  c152::DisplayTransition displaySleep(bool sleeping) override {
    if (sleeping) { ++sleeps; return {!failSleep}; }
    ++wakes; return {!failWake, 150};
  }
  c152::BatteryStatus readBattery() override { ++reads; return battery; }
  bool usbHostPresent() override { return usb; }
  void feedback(uint16_t ms) override { ++feedbacks; HostPlatform::feedback(ms); }
  device_clock::Reading writeClock(int64_t) override { ++clockWrites; return {}; }
  void brightness(uint8_t value) override {
    if (observeWake && value) {
      check(M5.Display.brightness == 0, "wake redraw happens before restoring brightness");
      check(M5.Display.publishedFrames().size() == 1, "wake publishes exactly one complete frame before lighting");
      litFrame = M5.Display.framebuffer();
    }
    HostPlatform::brightness(value);
  }
};
void ready(MemoryStore& store, unsigned steps = 0) {
  Engine engine(store); check(engine.begin() == CommandResult::Applied, "power fixture boots");
  engine.chooseStarter(1); step(engine, 1, steps); engine.checkpoint();
}
void wake(Runtime& runtime) {
  edge(runtime, false, true); idle(runtime, 160); frame(runtime);
  check(!runtime.dimmed(), "display wakes after settling and release");
}
void observeWake(PowerPlatform& platform) {
  M5.Display.beginObservation(); platform.observeWake = true;
}
void endWake(PowerPlatform& platform) {
  M5.Display.endObservation(); platform.observeWake = false;
}
void autoSleepAndWalk() {
  MemoryStore store; ready(store, 4997);
  PowerPlatform platform; platform.battery.percent = 67;
  Runtime runtime(store, platform); const uint32_t began = host_time_ms; start(runtime);
  idle(runtime, 29960);
  check(host_time_ms - began == 29980 && !runtime.dimmed(), "automatic sleep waits the full thirty seconds");
  frame(runtime);
  check(runtime.dimmed() && platform.sleeps == 1 && M5.Display.brightness == 0, "thirty seconds sends one sleep command");
  const unsigned reads = platform.reads, commits = store.commits;
  M5.Display.beginObservation(); walk(runtime, platform, 8); idle(runtime, 30000); M5.Display.endObservation();
  const auto& save = runtime.engine().view();
  check(save.totalSteps >= 5004 && save.progress[0].walked == save.totalSteps, "asleep detector keeps counting and cultivating");
  check(save.phase == Phase::EncounterReady && save.encounter.id == 1, "walking asleep creates the encounter once");
  check(platform.feedbacks == 1 && runtime.dimmed(), "durable encounter vibrates without lighting screen");
  check(store.commits > commits && platform.reads == reads && platform.sleeps == 1, "sleep keeps checkpointing without battery reads or repeated commands");
  check(M5.Display.publishedFrames().empty(), "asleep runtime never submits display frames");
  Save restored; check(store.load(restored).status == LoadStatus::Loaded && restored.totalSteps == save.totalSteps,
                       "steps earned asleep survive production codec reload");
  observeWake(platform); wake(runtime); endWake(platform);
  check(runtime.screen().selected == 0 && runtime.screen().page == Page::Home, "wake key does not open pending encounter");
  check(platform.reads == reads + 1 && platform.wakes == 1, "wake refreshes battery once");
  check(runtime.metrics().maxSampleGapMs <= 40 && runtime.metrics().qualityGaps == 0,
        "150ms wake settling does not interrupt synthetic sampling");
  screenshot(runtime, "power-home-67");
}
void manualAndBoundary() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime);
  for (unsigned i = 0; i < 6; ++i) edge(runtime, false, true);
  buttons(true, true); frame(runtime); idle(runtime, 400);
  check(runtime.dimmed() && platform.sleeps == 1 && platform.wakes == 0, "held manual confirmation cannot immediately wake itself");
  buttons(false, false); frame(runtime); frame(runtime);
  buttons(false, true); frame(runtime); const uint32_t wakeAt = host_time_ms;
  observeWake(platform);
  buttons(false, false); frame(runtime);
  buttons(true, false); frame(runtime); idle(runtime, 100); frame(runtime, 9);
  check(host_time_ms - wakeAt == 149 && runtime.dimmed() && M5.Display.brightness == 0,
        "display remains dark at 149ms even when a second key is pressed");
  frame(runtime, 1); endWake(platform);
  check(!runtime.dimmed() && runtime.screen().selected == 6, "150ms boundary lights without executing waiting-period key");
  idle(runtime, 300); buttons(false, false); frame(runtime); frame(runtime);
  check(runtime.screen().selected == 6, "held wake key and release cannot move selection");
  edge(runtime, true, false);
  check(runtime.screen().selected == 5, "first fresh key after release moves once");
  screenshot(runtime, "power-home-menu");
  buttons(false, true); frame(runtime); idle(runtime, 30200);
  check(!runtime.dimmed(), "continuous held input prevents automatic sleep");
  buttons(false, false); frame(runtime);
}
void clockDraft() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 4); select(runtime, 1); select(runtime, 0); edge(runtime, false, true);
  const auto draft = runtime.screen(); const unsigned writes = platform.clockWrites;
  idle(runtime, 30020); check(runtime.dimmed() && draft.editing, "clock editor can sleep with an unsaved draft");
  tap(runtime, 233, 390); idle(runtime, 160); frame(runtime);
  const auto& after = runtime.screen();
  check(!runtime.dimmed() && after.page == Page::Clock && after.editing && after.selected == draft.selected &&
        after.civil.year == draft.civil.year && after.civil.month == draft.civil.month &&
        after.civil.day == draft.civil.day && after.offset == draft.offset && after.clockMode == draft.clockMode,
        "wake touch preserves editor field and draft instead of confirming it");
  check(platform.clockWrites == writes && !runtime.engine().view().clock.pending.active,
        "sleep and wake never persist a clock draft");
  screenshot(runtime, "power-clock-draft");
}
void pausedAndActive() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 5); const unsigned reads = platform.reads;
  for (unsigned elapsed = 0; elapsed < 6500; elapsed += 20) {
    frame(runtime); check(!runtime.dimmed(), "active practice never sleeps");
  }
  check(runtime.screen().page == Page::Practice && platform.sleeps == 0 && platform.reads == reads,
        "active practice defers battery I/O past five seconds");
  tap(runtime, 233, 30); idle(runtime, 1700);
  check(runtime.screen().page == Page::Pause && runtime.practiceView().paused, "practice reaches a safe pause boundary");
  const auto paused = runtime.practiceView();
  idle(runtime, 30020); check(runtime.dimmed(), "paused practice sleeps");
  wake(runtime);
  check(runtime.screen().page == Page::Pause && runtime.practiceView().paused &&
        runtime.practiceView().cueIndex == paused.cueIndex && runtime.practiceView().playerHp == paused.playerHp,
        "wake does not resume or alter a paused battle");
  screenshot(runtime, "power-paused");
}
void throwWaiting() {
  MemoryStore store; Save save;
  save.bootGeneration = 1; save.starter = save.partner = 1; setOwned(save, 1); setRegistered(save, 1);
  save.rosterRevision = 1; save.totalSteps = save.undatedSteps = 5000; save.nextEncounterId = 2;
  save.phase = Phase::ThrowReady; save.result = Result::Won;
  save.encounter.id = 1; save.encounter.species = save.encounter.partner = 1;
  check(validate(save) && store.commit(save) == CommitStatus::Committed, "pending victory fixture validates");
  PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint64_t balls = availableBalls(runtime.engine().view());
  idle(runtime, 30020); check(runtime.dimmed(), "waiting to throw can sleep");
  wake(runtime);
  check(runtime.screen().page == Page::Throw && !runtime.engine().throwView().active &&
        availableBalls(runtime.engine().view()) == balls && runtime.engine().view().encounter.id == 1,
        "wake preserves victory without preparing or spending a ball");
  screenshot(runtime, "power-throw-waiting");
  edge(runtime, false, true); check(runtime.engine().throwView().active, "fresh throw input still works");
  const unsigned reads = platform.reads, sleeps = platform.sleeps;
  while (runtime.engine().throwView().active) { frame(runtime); check(!runtime.dimmed(), "active throw remains visible"); }
  check(platform.sleeps == sleeps && platform.reads == reads && availableBalls(runtime.engine().view()) == balls - 1,
        "throw timeout spends exactly once without power I/O during timing");
}
void viewerWakeInputs() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 3); frame(runtime); edge(runtime, false, true); idle(runtime, 1000);
  auto& state = viewer_screen::g_state;
  const unsigned species = state.sprite_index; const uint32_t origin = state.animation_started_ms;
  check(species != 0, "viewer is on a noninitial species");
  for (unsigned gesture = 0; gesture < 4; ++gesture) {
    idle(runtime, 30020); check(runtime.dimmed(), "viewer sleeps");
    const unsigned feedbacks = platform.feedbacks;
    auto& touch = M5.Touch.detail;
    touch.base_x = touch.x = 233; touch.base_y = touch.y = gesture == 0 ? 30 : 233;
    touch.pressed = true; frame(runtime); observeWake(platform);
    if (gesture == 1) { touch.pressed = false; touch.released = touch.clicked = true; frame(runtime); touch = {}; }
    if (gesture == 2) { buttons(false, true); frame(runtime); buttons(true, true); frame(runtime); }
    idle(runtime, gesture == 2 ? 120 : gesture == 1 ? 140 : 160);
    endWake(platform);
    check(!runtime.dimmed() && runtime.screen().page == Page::Viewer && state.sprite_index == species &&
          state.animation_started_ms == origin, "wake redraw preserves viewer species and animation origin");
    const auto expected = [&]() { auto copy = state; viewer::update(copy, data::kSprites[species], host_time_ms); return copy; }();
    check(state.frame_index == expected.frame_index, "wake frame follows elapsed animation time");
    if (gesture != 1) {
      touch.pressed = false; touch.released = true; touch.clicked = gesture != 3;
      if (gesture == 3) touch.x = 100;
      frame(runtime);
    }
    touch = {}; buttons(false, false); frame(runtime); frame(runtime);
    check(runtime.screen().page == Page::Viewer && state.sprite_index == species && platform.feedbacks == feedbacks,
          "wake top click, reaction, dual keys and swipe release cannot leak into viewer");
  }
  tap(runtime, 233, 233);
  check(platform.feedbacks > 0 && state.reacting, "fresh viewer interaction works after wake gate releases");
  idle(runtime, 1000); screenshot(runtime, "power-viewer");
}
void batteryStates() {
  MemoryStore store; ready(store); PowerPlatform platform; platform.battery.percent = 100;
  Runtime runtime(store, platform); start(runtime);
  screenshot(runtime, "battery-full"); openMenu(runtime);
  const unsigned initialReads = platform.reads; frame(runtime);
  check(platform.reads == initialReads, "battery is not polled each loop");
  const int levels[] = {0, 1, 20, 21, 99, 100, -1, 101};
  for (int level : levels) {
    platform.battery.percent = level; edge(runtime, false, true); idle(runtime, 5000);
    check(runtime.screen().battery.percent == (level >= 0 && level <= 100 ? level : -1),
          "zero is valid while invalid battery estimates remain unknown");
    check(M5.Display.readPixel(198, 16) == (level >= 0 && level <= 20 ? theme::kRed : theme::kText),
          "battery outline changes only at low-battery threshold");
  }
  screenshot(runtime, "battery-unknown");
  platform.battery.percent = 9; platform.battery.charge = c152::ChargeState::Charging;
  edge(runtime, false, true); idle(runtime, 5000); screenshot(runtime, "battery-low-charging");
  check(M5.Display.readPixel(209, 16) == theme::kGold, "charging symbol is visible independently of percentage");
  platform.battery.percent = -1; edge(runtime, false, true); idle(runtime, 5000);
  screenshot(runtime, "battery-unknown-charging");
  check(M5.Display.readPixel(209, 16) == theme::kGold, "unknown percentage preserves valid charging state");
  platform.battery.percent = 0; platform.battery.charge = c152::ChargeState::Unknown;
  edge(runtime, false, true); idle(runtime, 5000); screenshot(runtime, "battery-zero");
  check(M5.Display.readPixel(209, 16) == theme::kBackground, "charging symbol clears when status becomes unknown");
  select(runtime, 3); frame(runtime);
  platform.battery.percent = 100; idle(runtime, 5000); screenshot(runtime, "battery-viewer-full");
  platform.battery.percent = 9; idle(runtime, 5000); screenshot(runtime, "battery-viewer-low");
  check(M5.Display.readPixel(198, 16) == theme::kRed, "viewer receives battery updates without species changes");
}
void failedCommandsAndSave() {
  MemoryStore store; ready(store); PowerPlatform platform; platform.failSleep = true;
  Runtime runtime(store, platform); start(runtime); openMenu(runtime); select(runtime, 6);
  check(!runtime.dimmed() && M5.Display.brightness == 96 && runtime.metrics().displayErrors == 1,
        "rejected sleep restores visible UI and records failure");
  platform.failSleep = false; select(runtime, 6); wake(runtime);
}
void wakeFailureAndSaveRetry() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 6); platform.failWake = true; edge(runtime, false, true); idle(runtime, 180);
  check(runtime.dimmed() && platform.wakes == 1 && runtime.metrics().displayErrors == 1,
        "failed wake stays dark without busy retries");
  platform.failWake = false; wake(runtime); check(platform.wakes == 2, "fresh wake retries failed command");
  select(runtime, 6); store.fail = true; walk(runtime, platform, 4); idle(runtime, 30020);
  check(runtime.dimmed() && runtime.engine().savePending(), "sleep preserves a failed checkpoint for retry");
  const uint64_t total = runtime.engine().view().totalSteps;
  observeWake(platform); wake(runtime); endWake(platform);
  check(runtime.screen().page == Page::Error && runtime.engine().savePending(), "wake draws current storage error without retrying it");
  screenshot(runtime, "power-save-error");
  store.fail = false; select(runtime, 0);
  check(!runtime.engine().savePending() && runtime.engine().view().totalSteps == total,
        "explicit retry commits the same step result after wake");
}
void viewerErrorWake() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 3); frame(runtime); idle(runtime, 30020);
  viewer_screen::g_ready = false;
  observeWake(platform); wake(runtime); endWake(platform);
  check(runtime.metrics().displayErrors == 1 && glyph("画面不可用，请重启", 290, theme::kRed),
        "viewer render failure redraws an error before lighting the display");
  viewer_screen::g_ready = true;
}
void pocketRest() {
  MemoryStore store; ready(store, 4997);
  PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(!runtime.pocketIdle() && runtime.nextSampleMs(host_time_ms) == 20,
        "visible runtime reports the sample cadence but does not enter pocket rest");
  idle(runtime, 30020);
  check(runtime.pocketIdle() && runtime.nextSampleMs(host_time_ms) == 20,
        "a fresh asleep sample may rest 20ms and still leaves 40ms before the cadence limit");
  host_time_ms += 1;
  check(runtime.pocketIdle() && runtime.nextSampleMs(host_time_ms) == 20,
        "rest stays capped at the 20ms sample cadence");
  host_time_ms += 50;
  check(runtime.nextSampleMs(host_time_ms) == 9,
        "rest shrinks so wake latency cannot reach the 100ms step gap");
  host_time_ms += 9;
  check(runtime.nextSampleMs(host_time_ms) == 0,
        "no rest remains inside the 40ms cadence margin");
  host_time_ms -= 60;
  walk(runtime, platform, 8); idle(runtime, 1000);
  check(runtime.pocketIdle() && runtime.engine().view().phase == Phase::EncounterReady,
        "pocket rest remains available after an asleep encounter");
  platform.feedback(40);
  check(!runtime.pocketIdle(), "encounter vibration keeps the asleep loop awake");
  host_time_ms += 39; check(!runtime.pocketIdle(), "rest stays off until the motor duration elapses");
  host_time_ms += 1; check(runtime.pocketIdle(), "rest resumes when the motor stops and no save is pending");
  platform.usb = true; check(!runtime.pocketIdle(),
    "a USB host keeps the asleep loop awake so CDC survives");
  platform.usb = false; check(runtime.pocketIdle(),
    "unplugging restores pocket rest");
  store.fail = true; walk(runtime, platform, 4); idle(runtime, 30020);
  check(runtime.engine().savePending() && !runtime.pocketIdle(),
        "an unfinished save keeps the asleep loop awake for retry");
  store.fail = false; edge(runtime, false, true); idle(runtime, 180);
  check(!runtime.dimmed() && !runtime.pocketIdle(), "waking and visible display never enter pocket rest");
}
void wraparound() {
  host_time_ms = UINT32_MAX - 30050;
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  idle(runtime, 30020); check(runtime.dimmed(), "idle elapsed time works near millis wrap");
  wake(runtime); check(host_time_ms < 1000 && platform.wakes == 1, "150ms wake settling survives millis wrap");
}
}
int main() {
  c152::begin("POWER TEST");
  autoSleepAndWalk(); manualAndBoundary(); clockDraft(); pausedAndActive(); throwWaiting();
  viewerWakeInputs(); batteryStates(); failedCommandsAndSave(); wakeFailureAndSaveRetry(); viewerErrorWake();
  pocketRest(); wraparound();
  std::printf("%u power Runtime/Button_Class/pixel checks, 0 failures\n", checks);
}
