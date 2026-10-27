#include "support/home_ui/pixel_checks.hpp"
#include "support/home_ui/touch_cases.hpp"
#include "support/home_ui/menu_cases.hpp"
#include "support/home_ui/depth_cases.hpp"
#include <cstring>

void animationContinuity() {
  MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint32_t origin = host_time_ms; const auto& sprite = data::kSprites[0];
  spritePixels(1, origin, true);
  if (sprite.frames[0].duration_ms > 1) frame(runtime, sprite.frames[0].duration_ms - 1);
  spritePixels(1, origin, true);
  M5.Display.beginObservation(); frame(runtime, 1); M5.Display.endObservation();
  check(M5.Display.publishedFrames().size() == 1, "original frame deadline publishes without waiting 500ms");
  spritePixels(1, origin, true);
  for (unsigned i = 1; i < std::min<unsigned>(sprite.frame_count, 24); ++i) {
    frame(runtime, sprite.frames[i].duration_ms); spritePixels(1, origin, true);
  }
  openMenu(runtime); idle(runtime, 380); select(runtime, 9); frame(runtime); home(runtime); spritePixels(1, origin, true);
  openMenu(runtime); select(runtime, 1); select(runtime, 1); frame(runtime); home(runtime); spritePixels(1, origin);
  openMenu(runtime); select(runtime, 3); frame(runtime); idle(runtime, 240); tap(runtime, 233, 30); frame(runtime);
  home(runtime); spritePixels(1, origin);
  openMenu(runtime); select(runtime, 5); frame(runtime);
  check(runtime.screen().page == Page::Practice, "real menu starts practice");
  idle(runtime, 2500); tap(runtime, 233, 30);
  for (unsigned i = 0; runtime.screen().page == Page::Practice && i < 200; ++i) frame(runtime);
  check(runtime.screen().page == Page::Pause && runtime.screen().practice, "practice pauses through actual touch input");
  select(runtime, 1); frame(runtime); home(runtime); spritePixels(1, origin);
  screenshot(runtime, "home-after-practice"); spritePixels(1, origin);
}
class ClockPlatform : public HostPlatform {
 public:
  int64_t utc = 1792713598LL;
  uint32_t setAt = host_time_ms;
  bool valid = true;
  device_clock::Reading readClock() override {
    device_clock::Reading r = {}; r.valid = valid; r.utcSeconds = utc + (now() - setAt) / 1000; return r;
  }
  c152::DisplayTransition displaySleep(bool sleeping) override { return {true, uint16_t(sleeping ? 0 : 150)}; }
};
void calibrated(MemoryStore& store, ClockPlatform& platform) {
  ready(store); Engine engine(store); check(engine.begin() == CommandResult::Applied, "dated fixture reloads");
  ClockReading target; target.trusted = true; target.utcSeconds = platform.utc;
  target.dayKey = 20261022; target.bootGeneration = engine.view().bootGeneration; target.monotonicMs = host_time_ms;
  check(engine.beginClockChange(1, ClockChangeMode::Normal, target) == CommandResult::Applied &&
        engine.finishClockChange(1, target) == CommandResult::Applied, "clock fixture uses public calibration transaction");
}
void todayLine(Runtime& runtime) {
  const auto& today = runtime.engine().view().today;
  char digits[24], text[32];
  std::snprintf(digits, sizeof(digits), "%llu", static_cast<unsigned long long>(today.steps));
  const int length = strlen(digits);
  int groups = (length - 1) / 3, out = 0;
  for (int index = 0; index < length; ++index) {
    text[out++] = digits[index];
    if (groups && index == length - groups * 3 - 1) { text[out++] = ','; --groups; }
  }
  text[out] = '\0';
  M5.Display.setFont(&fonts::Font4);
  const int width = M5.Display.textWidth(text);
  if (width > 180) {
    check(glyph(today.complete ? "今日 · 步" : "今日已记录 · 步", 380, theme::kMuted),
          "long today values keep their unit above the number");
    const lgfx::IFont* numberFont = width > 280 ? static_cast<const lgfx::IFont*>(&fonts::Font0)
                                                : static_cast<const lgfx::IFont*>(&fonts::Font2);
    check(fontLine(text, 407, theme::kText, numberFont),
          "long today values use a complete second line");
  } else {
    check(numberLine(text, 393, theme::kText, today.complete ? 237 : 250),
          "visible today number equals live Engine bucket");
    check(glyph(today.complete ? "今日" : "今日已记录", 396, theme::kMuted, today.complete ? 158 : 139),
          "today label includes record completeness");
  }
}
void datesAndLiveSteps() {
  {
    MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    check(textLine("今日 --", 393, theme::kText), "unknown RTC displays no fabricated daily number");
    screenshot(runtime, "home-today-unknown");
  }
  MemoryStore store; ClockPlatform platform; calibrated(store, platform);
  Runtime runtime(store, platform); start(runtime);
  check(runtime.engine().view().clock.trusted && !runtime.engine().view().today.complete,
        "calibration partway through a day is explicitly incomplete");
  todayLine(runtime); screenshot(runtime, "home-today-partial");
  idle(runtime, 2400);
  check(runtime.engine().view().today.dayKey == 20261023 && runtime.engine().view().today.complete,
        "trusted midnight starts a complete new daily bucket");
  const unsigned commits = store.commits; walk(runtime, platform, 4);
  check(runtime.engine().view().today.steps > 0, "real detector produces dated steps on home");
  Save loaded; check(store.load(loaded).status == LoadStatus::Loaded &&
                     loaded.today.steps < runtime.engine().view().today.steps && store.commits == commits,
                     "home shows live steps before a durable checkpoint");
  todayLine(runtime); screenshot(runtime, "home-today-complete-live");
  platform.valid = false; idle(runtime, 1100);
  check(!runtime.engine().view().clock.trusted && runtime.engine().view().today.steps > 0,
        "RTC loss retains old bucket for the stale-number regression");
  check(textLine("今日 --", 393, theme::kText), "stale trusted-day bucket is hidden after RTC loss");
  screenshot(runtime, "home-today-stale-hidden");
  platform.valid = true; idle(runtime, 1100); todayLine(runtime);
  check(!runtime.engine().view().today.complete, "recovered same day remains explicitly partial");
  platform.utc -= 300; idle(runtime, 1100);
  check(runtime.engine().view().clock.anomaly && textLine("今日 --", 393, theme::kText),
        "backward RTC anomaly cannot relabel the retained bucket as today");
}
void maximumToday() {
  MemoryStore store; ClockPlatform platform; platform.utc = 1792670400LL; calibrated(store, platform);
  Save save; check(store.load(save).status == LoadStatus::Loaded, "extreme-date fixture loads genuine calibrated bytes");
  save.totalSteps = save.explorationCredit = save.rewardBaseSteps = UINT64_MAX;
  save.today.steps = save.bestDaySteps = save.progress[0].walked = UINT64_MAX;
  save.today.complete = true;
  const uint64_t daily[] = {1000, 3000, 5000}, lifetime[] = {10000, 50000, 100000};
  for (unsigned i = 0; i < 3; ++i) {
    Achievement dailyAward; dailyAward.reached = true;
    auto& stamp = dailyAward.first; stamp.timeKnown = true; stamp.dayComplete = true;
    stamp.utcSeconds = platform.utc; stamp.dayKey = 20261022; stamp.bootGeneration = save.bootGeneration;
    stamp.calibrationEpoch = save.clock.calibrationEpoch; stamp.totalSteps = stamp.dailySteps = daily[i];
    save.today.milestones[i] = save.dailyFirst[i] = dailyAward;
    Achievement lifeAward = dailyAward; lifeAward.first.totalSteps = lifeAward.first.dailySteps = lifetime[i];
    save.lifetime[i] = lifeAward;
  }
  check(validate(save) && store.commit(save) == CommitStatus::Committed,
        "UINT64_MAX fixture passes real semantic validation and encoder");
  Runtime runtime(store, platform); start(runtime); home(runtime); todayLine(runtime); roundPixels();
  check(runtime.engine().view().today.steps == UINT64_MAX, "maximum daily number survives startup without truncation");
  screenshot(runtime, "home-today-uint64-max"); todayLine(runtime);
  openMenu(runtime); screenshot(runtime, "menu-uint64-max");
  select(runtime, 4); screenshot(runtime, "resources-uint64-max");
  check(glyph("18446744073709551615 / 7500 步", 156, theme::kText),
        "maximum cultivation balance remains fully readable on its own resources line");
  tap(runtime, 233, 52); select(runtime, 2); select(runtime, 0); screenshot(runtime, "partner-uint64-max");
  check(glyph("陪伴 18446744073709551615 步", 80, theme::kMint) &&
        glyph("可用 18446744073709551615 步", 165, theme::kMint),
        "maximum partner counters remain fully readable without overlapping the preview");
}
void sleepContinuity() {
  MemoryStore store; ready(store); ClockPlatform platform; platform.valid = false;
  Runtime runtime(store, platform); start(runtime); const uint32_t origin = host_time_ms;
  idle(runtime, 30020); check(runtime.dimmed(), "animation is not user activity and home sleeps after thirty seconds");
  buttons(true, false); frame(runtime); buttons(true, true); frame(runtime); idle(runtime, 160);
  home(runtime); check(!runtime.dimmed(), "wake settles without opening menu");
  buttons(false, true); frame(runtime); home(runtime); settle(runtime); spritePixels(1, origin, true);
  screenshot(runtime, "home-after-wake"); spritePixels(1, origin, true);
  openMenu(runtime); select(runtime, 9); frame(runtime); home(runtime);
}
void saveFailure() {
  MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 1); const auto bytes = store.bytes;
  store.fail = true; select(runtime, 1); frame(runtime);
  check(runtime.screen().page == Page::Error && runtime.engine().savePending() &&
        runtime.engine().view().route == Route::Forest && store.bytes == bytes,
        "failed route save exposes error and does not publish the uncommitted scene");
  store.fail = false; const unsigned commits = store.commits; select(runtime, 0); frame(runtime); home(runtime);
  check(runtime.engine().view().route == Route::Coast && store.commits == commits + 1,
        "explicit retry commits the original route change once");
  Engine restarted(store); check(restarted.begin() == CommandResult::Applied && restarted.view().route == Route::Coast,
                                "route survives real Engine restart and codec reload");
}
void clockPendingRestart() {
  MemoryStore store; ClockPlatform platform; calibrated(store, platform);
  {
    Engine engine(store); check(engine.begin() == CommandResult::Applied, "pending fixture reloads calibrated state");
    ClockReading target; target.trusted = true; target.utcSeconds = platform.utc;
    target.dayKey = 20261022; target.bootGeneration = engine.view().bootGeneration;
    target.calibrationEpoch = engine.view().clock.calibrationEpoch; target.monotonicMs = host_time_ms;
    check(engine.beginClockChange(2, ClockChangeMode::Normal, target) == CommandResult::Applied,
          "pending clock transaction is durably started through Engine");
  }
  Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::DateWelcome && runtime.engine().view().clock.pending.active,
        "pending clock restoration retains setup priority over home");
  select(runtime, 1); frame(runtime); home(runtime);
  check(textLine("今日 --", 393, theme::kText), "explicit skip with pending clock never displays retained bucket");
  screenshot(runtime, "home-clock-pending-skipped");
}
void battleContinuity() {
  MemoryStore store; ready(store);
  { Engine engine(store); engine.begin(); step(engine, 1, 5000); engine.checkpoint(); }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint32_t origin = host_time_ms;
  check(runtime.engine().view().phase == Phase::EncounterReady, "saved encounter starts on exploration");
  check(glyph("发现宝可梦 · 侧键打开菜单", 428, theme::kGold), "home exposes pending encounter without opening it");
  openMenu(runtime); select(runtime, 0); select(runtime, 0); frame(runtime);
  check(runtime.screen().page == Page::Battle, "real encounter starts formal battle");
  idle(runtime, 180); tap(runtime, 233, 30);
  for (unsigned i = 0; runtime.screen().page == Page::Battle && i < 150; ++i) frame(runtime);
  check(runtime.screen().page == Page::Pause && !runtime.screen().practice, "formal battle pauses at a safe cue boundary");
  select(runtime, 1); frame(runtime);
  check(runtime.screen().page == Page::Result, "formal retreat preserves result-page priority");
  select(runtime, 0); frame(runtime); home(runtime); spritePixels(1, origin, true);
  screenshot(runtime, "home-after-battle"); spritePixels(1, origin, true);
}
void animationWraparound() {
  host_time_ms = UINT32_MAX - 400;
  MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint32_t origin = host_time_ms;
  idle(runtime, 700); home(runtime);
  check(host_time_ms < 1000, "home animation crosses millis wrap"); spritePixels(1, origin, true);
  openMenu(runtime); select(runtime, 9); frame(runtime); home(runtime); spritePixels(1, origin, true);
}
void allPartners() {
  const std::vector<uint16_t> representatives = {1, 4, 7, 6, 42, 122, 139, 149,
                                                 152, 157, 160, 181, 197, 208, 245, 249, 251};
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    MemoryStore store; modernOwned(store, {id}, id); HostPlatform platform;
    Runtime runtime(store, platform); start(runtime); const uint32_t origin = host_time_ms;
    home(runtime); check(runtime.engine().view().partner == id, "legal versioned collection restores requested partner");
    spritePixels(id, origin, id == 1); roundPixels();
    const uint16_t contact = theme::rgb(0x6C, 0x68, 0x48);
    lgfx::LGFX_Sprite shadow;
    shadow.setColorDepth(16);
    check(shadow.createSprite(468, 468), "allocate fixed ground shadow reference");
    shadow.fillEllipse(233, 305, 40, 6, contact);
    for (int y = 299; y <= 311; ++y) for (int x = 193; x <= 273; ++x)
      if (shadow.readPixel(x, y) == contact) {
        const uint16_t actual = M5.Display.readPixel(x, y);
        check(actual == contact || actual != kTransparentKey,
              "the actor moves down while the ground shadow stays fixed");
      }
    shadow.deleteSprite();
    if (id == 1) {
      check(M5.Display.readPixel(223, 310) == contact &&
            M5.Display.readPixel(265, 308) == contact,
            "Bulbasaur front and rear feet touch the shared contact shadow");
    }
    if (std::find(representatives.begin(), representatives.end(), id) != representatives.end()) {
      char name[40]; std::snprintf(name, sizeof(name), "home-partner-%03u", unsigned(id)); screenshot(runtime, name);
      spritePixels(id, origin, id == 1);
    }
  }
}
int main() {
  c152::begin("HOME TEST");
  homeTouch(); openingGestures(); ninthItem(); routesAndPartner(); animationContinuity();
  datesAndLiveSteps(); maximumToday(); sleepContinuity(); saveFailure(); clockPendingRestart();
  battleContinuity(); sceneDepthMotion(); sceneDepthNavigationAndSleep(); allPartners(); animationWraparound();
  std::printf("%u home Runtime/Button_Class/M5GFX checks, 0 failures\n", checks);
}
