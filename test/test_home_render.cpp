#include "support/progression_ui.hpp"
#include "support/rendered_text.hpp"
#include "battle_presentation.hpp"
#include "scene_backgrounds_generated.hpp"
#include "sprites_generated.hpp"
#include <cmath>
#include <vector>
#include <zlib.h>
using namespace progression_ui;

namespace {
void savePng(const char* name) {
  constexpr int kSize = 468;
  std::vector<uint8_t> raw(size_t(kSize) * (1 + kSize * 3));
  for (int y = 0; y < kSize; ++y) {
    uint8_t* row = raw.data() + size_t(y) * (1 + kSize * 3);
    row[0] = 0;
    for (int x = 0; x < kSize; ++x) {
      const uint16_t p = M5.Display.readPixel(x, y);
      row[1 + x * 3 + 0] = uint8_t((p >> 11) * 255 / 31);
      row[1 + x * 3 + 1] = uint8_t(((p >> 5) & 63) * 255 / 63);
      row[1 + x * 3 + 2] = uint8_t((p & 31) * 255 / 31);
    }
  }
  uLongf zlen = compressBound(uLong(raw.size()));
  std::vector<uint8_t> zbuf(zlen);
  check(compress2(zbuf.data(), &zlen, raw.data(), uLong(raw.size()), Z_BEST_SPEED) == Z_OK, "compress PNG");
  const std::string path = verificationOutput((std::string(name) + ".png").c_str());
  FILE* f = std::fopen(path.c_str(), "wb");
  check(f != nullptr, "open PNG");
  const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  std::fwrite(sig, 1, 8, f);
  auto chunk = [&](const char tag[4], const uint8_t* data, size_t n) {
    const uint8_t len[4] = {uint8_t(n >> 24), uint8_t(n >> 16), uint8_t(n >> 8), uint8_t(n)};
    std::fwrite(len, 1, 4, f); std::fwrite(tag, 1, 4, f);
    if (n) std::fwrite(data, 1, n, f);
    uLong crc = crc32(0L, reinterpret_cast<const Bytef*>(tag), 4);
    if (n) crc = crc32(crc, data, uInt(n));
    const uint8_t tail[4] = {uint8_t(crc >> 24), uint8_t(crc >> 16), uint8_t(crc >> 8), uint8_t(crc)};
    std::fwrite(tail, 1, 4, f);
  };
  const uint8_t hdr[13] = {0, 0, kSize >> 8, kSize & 0xff, 0, 0, kSize >> 8, kSize & 0xff, 8, 2, 0, 0, 0};
  chunk("IHDR", hdr, 13); chunk("IDAT", zbuf.data(), zlen); chunk("IEND", nullptr, 0);
  std::fclose(f);
}
void checkCircleBoundary(const char* label) {
  for (int y = 0; y < 468; ++y) {
    const int dy = y - c152::kScreenCenter;
    for (int x = 0; x < 468; ++x) {
      const int dx = x - c152::kScreenCenter;
      if (dx * dx + dy * dy > c152::kVisibleRadius * c152::kVisibleRadius &&
          M5.Display.readPixel(x, y) != TFT_BLACK) {
        std::fprintf(stderr, "circle overflow in %s at (%d,%d)\n", label, x, y);
        check(false, "render stays inside round screen visible radius");
      }
    }
  }
}
std::vector<uint16_t> baseCirclePixels() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(c152::kScreenCenter, c152::kScreenCenter, 227, theme::kBackground);
  std::vector<uint16_t> pixels(468u * 468u);
  for (int y = 0; y < 468; ++y)
    for (int x = 0; x < 468; ++x)
      pixels[size_t(y) * 468u + size_t(x)] = M5.Display.readPixel(x, y);
  return pixels;
}
void checkSceneSpanEdges(Route route, bool dim, int dx, const std::vector<uint16_t>& baseCircle) {
  auto expectedAt = [&](int x, int screenY) -> uint16_t {
    const int sourceCol = x - (5 + dx);
    if (sourceCol < 0 || sourceCol >= int(data::kSceneWidth * data::kSceneScale))
      return baseCircle[size_t(screenY) * 468u + size_t(x)];
    const unsigned sx = unsigned(sourceCol) / data::kSceneScale;
    const unsigned sy = unsigned(screenY - 92) / data::kSceneScale;
    if (!sceneEdgeVisible(sx, sy)) return theme::kBackground;
    const auto& src = dim ? data::kRegionScenesDim[regionIndex(route)] : data::kRegionScenes[regionIndex(route)];
    return src.pixels[sy * data::kSceneWidth + sx];
  };
  for (int screenY = 92; screenY < 362; ++screenY) {
    const int v = screenY - c152::kScreenCenter;
    const int span = int(std::sqrt(float(c152::kVisibleRadius * c152::kVisibleRadius - v * v)));
    const int left = c152::kScreenCenter - span, right = c152::kScreenCenter + span;
    check(M5.Display.readPixel(left - 1, screenY) == TFT_BLACK, "left of scene circle stays black");
    check(M5.Display.readPixel(left, screenY) == expectedAt(left, screenY), "left scene edge matches span");
    if (span < c152::kVisibleRadius) {
      check(M5.Display.readPixel(right + 1, screenY) == TFT_BLACK, "right of scene circle stays black");
      check(M5.Display.readPixel(right, screenY) == expectedAt(right, screenY), "right scene edge matches span");
    }
  }
}
void commitEncounter(MemoryStore& store, Route route) {
  Save s;
  s.bootGeneration = 1; s.starter = s.partner = 1;
  setOwned(s, 1); setRegistered(s, 1); setRegistered(s, 25);
  s.rosterRevision = 1; s.encounterTarget = 300;
  s.totalSteps = s.undatedSteps = 5000; s.explorationSpent = 5000;
  s.nextEncounterId = 2; s.phase = Phase::EncounterReady;
  s.encounter.id = 1; s.encounter.species = 25; s.encounter.partner = 1; s.encounter.route = route;
  s.encounter.seed = 7;
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "encounter fixture commits");
}
void pressCue(Runtime& runtime, Action expected) {
  const uint32_t dt = host_time_ms < runtime.engine().battleView().targetMs
      ? runtime.engine().battleView().targetMs - host_time_ms : 0;
  M5.button_a_down = expected == Action::Dodge;
  M5.button_b_down = expected == Action::Attack;
  frame(runtime, dt);
  M5.button_a_down = M5.button_b_down = false;
}
void waitCue(Runtime& runtime, Action target, bool answerOther) {
  while (runtime.engine().battleView().active) {
    const auto bv = runtime.engine().battleView();
    if (!bv.consumed && !countdownDigit(bv, host_time_ms) && int32_t(bv.targetMs - host_time_ms) <= 20) {
      if (bv.expected == target) return;
      if (answerOther) { pressCue(runtime, bv.expected); frame(runtime, 20); continue; }
    }
    frame(runtime, 20);
  }
}
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
  savePng("home-today-complete-live");
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
  openMenu(runtime); select(runtime, MenuItem::Partner); select(runtime, 0); select(runtime, 0);
  check(rendered_text::contains("陪伴 18446744073709551615 步", theme::kMint) &&
        rendered_text::contains("可用 18446744073709551615 步", theme::kMint),
        "maximum partner counters remain fully readable");
  screenshot(runtime, "partner-uint64-max");
}
void sceneCacheAndCircleBounds() {
  const auto baseCircle = baseCirclePixels();
  Screens cached;
  check(cached.begin(), "cached Screens initializes buffers");
  const char* const regionNames[] = {"kanto", "johto", "hoenn", "sinnoh", "unova"};
  for (uint8_t r = 0; r < kRegionCount; ++r) {
    const Route route = regionAt(r);
    MemoryStore store; ClockPlatform platform; calibrated(store, platform);
    Save save; check(store.load(save).status == LoadStatus::Loaded, "region fixture loads");
    save.route = route;
    check(validate(save) && store.commit(save) == CommitStatus::Committed, "region fixture saves");
    Engine engine(store); check(engine.begin() == CommandResult::Applied, "region engine loads");
    ScreenState state; state.page = Page::Home;

    check(cached.draw(state, engine, {}, 1000), "static region first draw");
    const auto firstFrame = M5.Display.framebuffer();
    check(cached.draw(state, engine, {}, 1000), "static region cache hit");
    const auto secondFrame = M5.Display.framebuffer();
    check(firstFrame == secondFrame, "static region cache hit matches build");

    Screens fresh;
    check(fresh.begin() && fresh.draw(state, engine, {}, 1000), "fresh static region draw");
    check(secondFrame == M5.Display.framebuffer(), "cached region matches fresh draw");
    checkCircleBoundary(regionNames[r]);
    checkSceneSpanEdges(route, false, 0, baseCircle);
    char pngName[32]; std::snprintf(pngName, sizeof(pngName), "region-%s", regionNames[r]);
    savePng(pngName);
  }
  {
    MemoryStore store; ClockPlatform platform; calibrated(store, platform);
    Save save; check(store.load(save).status == LoadStatus::Loaded, "parallax fixture loads");
    save.route = Route::Hoenn;
    check(validate(save) && store.commit(save) == CommitStatus::Committed, "parallax fixture saves");
    Engine engine(store); check(engine.begin() == CommandResult::Applied, "parallax engine loads");
    ScreenState state; state.page = Page::Home;
    Screens reference; check(reference.begin(), "reference Screens begins");
    reference.advanceHome(engine.view().partner, 1000);
    for (uint32_t t : {1000u, 1100u, 1200u, 1300u}) {
      cached.advanceScene(Route::Hoenn, t, true);
      reference.advanceScene(Route::Hoenn, t, true);
    }
    check(cached.draw(state, engine, {}, 1300), "dynamic parallax draws");
    const auto dynCached = M5.Display.framebuffer();
    checkCircleBoundary("home-parallax");
    check(reference.draw(state, engine, {}, 1300), "fresh dynamic parallax draws");
    check(dynCached == M5.Display.framebuffer(), "dynamic parallax matches reference");

    MemoryStore encStore; commitEncounter(encStore, Route::Kanto);
    Engine encEngine(encStore); check(encEngine.begin() == CommandResult::Applied, "encounter engine loads");
    ScreenState encState; encState.page = Page::Encounter;
    check(cached.draw(encState, encEngine, {}, 1300), "dimmed encounter draws after parallax");
    const auto encFirst = M5.Display.framebuffer();
    check(cached.draw(encState, encEngine, {}, 1300), "dimmed encounter hits sceneCache_");
    check(encFirst == M5.Display.framebuffer(), "dimmed encounter cache hit matches build");
    Screens freshEnc; check(freshEnc.begin() && freshEnc.draw(encState, encEngine, {}, 1300), "fresh encounter draws");
    check(encFirst == M5.Display.framebuffer(), "dimmed encounter matches fresh draw");
  }
}

void battleShakeDodgeAndEvolution() {
  {
    const auto baseCircle = baseCirclePixels();
    MemoryStore store; commitEncounter(store, Route::Kanto);
    HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    openMenu(runtime); select(runtime, MenuItem::Encounter); select(runtime, 0);
    check(runtime.screen().page == Page::Battle, "enters battle page");

    waitCue(runtime, Action::Dodge, false);
    check(runtime.engine().battleView().active && runtime.engine().battleView().expected == Action::Dodge, "reaches dodge cue");
    pressCue(runtime, Action::Dodge); frame(runtime, 60);
    check(runtime.engine().battleView().lastTiming != Timing::Miss &&
          battleEffectActive(runtime.engine().battleView(), host_time_ms), "dodge effect active");
    checkCircleBoundary("battle-dodge-afterimage");
    bool sawOutline = false, sawTrack = false;
    for (int y = kPartnerY - 40; y < kPartnerY + 40; ++y)
      for (int x = 90; x < 126; ++x) {
        const uint16_t p = M5.Display.readPixel(x, y);
        sawOutline = sawOutline || p == theme::kOutline;
        sawTrack = sawTrack || p == theme::kTrack;
      }
    check(sawOutline && sawTrack, "dodge renders both afterimage silhouette tones");
    savePng("battle-dodge-afterimage");

    Screens verify; check(verify.begin() && verify.draw(runtime.screen(), runtime.engine(), runtime.engine().battleView(), host_time_ms), "fresh dodge draw");
    const auto verifyFrame = M5.Display.framebuffer();
    Screens second; check(second.begin() && second.draw(runtime.screen(), runtime.engine(), runtime.engine().battleView(), host_time_ms), "second dodge draw");
    check(verifyFrame == M5.Display.framebuffer() && second.draw(runtime.screen(), runtime.engine(), runtime.engine().battleView(), host_time_ms) && verifyFrame == M5.Display.framebuffer(),
          "dodge dual-slot redraw matches fresh draw");

    idle(runtime, 400);
    check(!battleEffectActive(runtime.engine().battleView(), host_time_ms), "dodge effect completes");
    checkCircleBoundary("battle-post-dodge");

    waitCue(runtime, Action::Attack, true);
    check(runtime.engine().battleView().active && runtime.engine().battleView().expected == Action::Attack, "reaches attack cue");
    pressCue(runtime, Action::Attack);
    check(runtime.engine().battleView().lastTiming == Timing::Perfect, "lands perfect attack for shake");
    const uint32_t settled = runtime.engine().battleView().settledMs;
    auto checkShake = [&](uint32_t age, int dx, const char* png) {
      if (host_time_ms < settled + age) frame(runtime, settled + age - host_time_ms);
      checkCircleBoundary(png);
      checkSceneSpanEdges(Route::Kanto, true, dx, baseCircle);
      savePng(png);
    };
    checkShake(100, -2, "battle-shake-minus2");
    checkShake(140, 2, "battle-shake-plus2");

    while (runtime.engine().battleView().active) {
      const auto bv = runtime.engine().battleView();
      if (!bv.consumed && !countdownDigit(bv, host_time_ms) && int32_t(bv.targetMs - host_time_ms) <= 20) {
        pressCue(runtime, bv.expected); frame(runtime, 20);
      } else frame(runtime, 20);
    }
    while (runtime.screen().page == Page::Battle) frame(runtime, 20);
    check(runtime.screen().page == Page::Throw, "battle victory enters Throw page");
    checkCircleBoundary("throw-aim"); savePng("throw-aim");
  }
  {
    MemoryStore store; Engine engine(store);
    check(engine.begin() == CommandResult::Applied && engine.chooseStarter(1) == CommandResult::Applied, "evolution fixture has starter");
    Screens cached; check(cached.begin(), "evolution Screens begins");
    ScreenState state; state.page = Page::EvolutionPreview;
    state.evolutionAnim.from = 1; state.evolutionAnim.to = 2; state.evolutionAnim.startMs = 1000;
    const struct { uint32_t offset; const char* png; } steps[] = {
      {200, nullptr}, {600, nullptr}, {900, "evolution-morph-from"}, {1100, "evolution-morph-to"},
      {1300, nullptr}, {2900, nullptr}, {3300, "evolution-reveal"},
    };
    for (const auto& step : steps) {
      const uint32_t now = state.evolutionAnim.startMs + step.offset;
      check(cached.draw(state, engine, {}, now), "cached evolution stage draws");
      const auto cachedFb = M5.Display.framebuffer();
      checkCircleBoundary(step.png ? step.png : "evolution-stage");
      if (step.png) savePng(step.png);
      Screens fresh; check(fresh.begin() && fresh.draw(state, engine, {}, now), "fresh evolution stage draws");
      check(cachedFb == M5.Display.framebuffer(), "cached dual-slot evolution matches fresh draw");
    }
  }
}
}
int main() {
  c152::begin("HOME TEST"); datesAndLiveSteps(); maximumToday();
  sceneCacheAndCircleBounds(); battleShakeDodgeAndEvolution();
  std::printf("%u home output checks, 0 failures\n", checks);
}
