#include "support/progression_ui.hpp"
#include "support/rendered_text.hpp"
using namespace progression_ui;

namespace {
class ClockPlatform : public HostPlatform {
 public:
  int64_t utc = 1792713598LL;
  uint32_t setAt = host_time_ms;
  bool valid = true;
  device_clock::Reading readClock() override {
    device_clock::Reading r = {}; r.valid = valid; r.utcSeconds = utc + (now() - setAt) / 1000; return r;
  }
};
void calibrated(MemoryStore& store, ClockPlatform& platform) {
  Engine engine(store);
  check(engine.begin() == CommandResult::Applied && engine.chooseStarter(1) == CommandResult::Applied,
        "home fixture has a starter");
  ClockReading target; target.trusted = true; target.utcSeconds = platform.utc;
  target.dayKey = 20260000u + 1022u; target.bootGeneration = engine.view().bootGeneration; target.monotonicMs = host_time_ms;
  check(engine.beginClockChange(1, ClockChangeMode::Normal, target) == CommandResult::Applied &&
        engine.finishClockChange(1, target) == CommandResult::Applied, "clock fixture is calibrated");
}
void datesAndLiveSteps() {
  MemoryStore store; ClockPlatform platform; calibrated(store, platform);
  Runtime runtime(store, platform); start(runtime);
  check(rendered_text::contains("今日已记录", theme::kMuted), "part-day calibration is visibly incomplete");
  screenshot(runtime, "home-today-partial");
  idle(runtime, 2400); walk(runtime, platform, 4);
  check(runtime.engine().view().today.steps > 0, "real detector produces dated steps on home");
  check(rendered_text::contains("今日", theme::kMuted) &&
        !rendered_text::contains("今日已记录", theme::kMuted), "new full day has its own today label");
  screenshot(runtime, "home-today-complete-live");
  platform.valid = false; idle(runtime, 1100);
  check(rendered_text::contains("今日 --", theme::kText), "RTC loss hides the retained stale day number");
  screenshot(runtime, "home-today-stale-hidden");
  platform.valid = true; idle(runtime, 1100);
  check(rendered_text::contains("今日已记录", theme::kMuted), "recovered day is visibly partial");
  platform.utc -= 300; idle(runtime, 1100);
  check(rendered_text::contains("今日 --", theme::kText), "backward RTC anomaly cannot present the retained bucket as today");
}
void maximumToday() {
  MemoryStore store; ClockPlatform platform; platform.utc = 1792670400LL; calibrated(store, platform);
  Save save; check(store.load(save).status == LoadStatus::Loaded, "extreme-date fixture loads");
  save.totalSteps = save.explorationCredit = save.rewardBaseSteps = UINT64_MAX;
  save.today.steps = save.bestDaySteps = save.progress[0].walked = UINT64_MAX;
  save.today.complete = true;
  const uint64_t daily[] = {1000, 3000, 5000}, lifetime[] = {10000, 50000, 100000};
  for (unsigned i = 0; i < 3; ++i) {
    Achievement dailyAward; dailyAward.reached = true;
    auto& stamp = dailyAward.first; stamp.timeKnown = true; stamp.dayComplete = true;
    stamp.utcSeconds = platform.utc; stamp.dayKey = 20260000u + 1022u; stamp.bootGeneration = save.bootGeneration;
    stamp.calibrationEpoch = save.clock.calibrationEpoch; stamp.totalSteps = stamp.dailySteps = daily[i];
    save.today.milestones[i] = save.dailyFirst[i] = dailyAward;
    Achievement lifeAward = dailyAward; lifeAward.first.totalSteps = lifeAward.first.dailySteps = lifetime[i];
    save.lifetime[i] = lifeAward;
  }
  check(validate(save) && store.commit(save) == CommitStatus::Committed, "maximum balance fixture validates");
  Runtime runtime(store, platform); start(runtime);
  check(rendered_text::contains("18,446,744,073,709,551,615", theme::kText, &fonts::Font0),
        "maximum daily counter is displayed without truncation");
  screenshot(runtime, "home-today-uint64-max");
  openMenu(runtime); select(runtime, 5);
  check(rendered_text::contains("18446744073709551615 / 7500 步", theme::kText),
        "maximum cultivation balance remains readable in resources");
  screenshot(runtime, "resources-uint64-max");
  tap(runtime, 233, 52); select(runtime, 2); select(runtime, 0);
  check(rendered_text::contains("陪伴 18446744073709551615 步", theme::kMint) &&
        rendered_text::contains("可用 18446744073709551615 步", theme::kMint),
        "maximum partner counters remain fully readable");
  screenshot(runtime, "partner-uint64-max");
}
}
int main() {
  c152::begin("HOME TEST"); datesAndLiveSteps(); maximumToday();
  std::printf("%u home output checks, 0 failures\n", checks);
}
