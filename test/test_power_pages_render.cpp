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
  openMenu(runtime); select(runtime, 5); select(runtime, 1); select(runtime, 0); edge(runtime, false, true);
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
  openMenu(runtime); select(runtime, 6);
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
  Runtime runtime(store, platform); start(runtime); openMenu(runtime); select(runtime, 7);
  check(!runtime.dimmed() && M5.Display.brightness > 0, "rejected sleep leaves a visible UI");
  platform.failSleep = false; select(runtime, 7); wake(runtime);
}
void wakeFailureAndSaveRetry() {
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 7); platform.failWake = true; M5.powerClick(); frame(runtime); idle(runtime, 180);
  check(runtime.dimmed(), "failed wake stays dark");
  platform.failWake = false; wake(runtime);
  select(runtime, 7); store.fail = true; walk(runtime, platform, 4); idle(runtime, 30020);
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
  openMenu(runtime); select(runtime, 4); frame(runtime); idle(runtime, 30020);
  viewer_screen::g_ready = false; wake(runtime);
  check(rendered_text::contains("画面不可用，请重启", theme::kRed),
        "viewer failure remains readable after waking");
  viewer_screen::g_ready = true;
}
void wraparound() {
  host_time_ms = UINT32_MAX - 30050;
  MemoryStore store; ready(store); PowerPlatform platform; Runtime runtime(store, platform); start(runtime);
  idle(runtime, 30020); check(runtime.dimmed(), "idle sleep survives clock rollover"); wake(runtime);
}
}
int main() {
  c152::begin("POWER PAGES TEST");
  autoSleepAndWalk(); clockDraft(); pausedAndActive(); failedCommandsAndSave(); wakeFailureAndSaveRetry(); viewerErrorWake(); wraparound();
  std::printf("%u power Runtime checks, 0 failures\n", checks);
}
