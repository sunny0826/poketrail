#include "driver.hpp"
#include "encounter_catalog.hpp"
#include "scene_backgrounds_generated.hpp"
#include "screen_theme.hpp"
#include <M5Unified.h>

namespace simulator {
namespace e2e {
namespace {
constexpr uint32_t kThrowSeed = 0x2468ace0u;

bool sceneShows(Route region, bool dim) {
  const auto& scene = (dim ? pokedex::data::kRegionScenesDim : pokedex::data::kRegionScenes)[regionIndex(region)];
  const int left = dim ? 70 : 60, right = dim ? 80 : 120, top = dim ? 180 : 110, bottom = dim ? 190 : 150;
  for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x)
    if (M5.Display.readPixel(x, y) != scene.pixels[(y - 92) / 2 * pokedex::data::kSceneWidth + (x - 5) / 2])
      return false;
  return true;
}

bool textAt(const char* text, int y, uint16_t color) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  if (!expected.createSprite(468, 28)) return false;
  expected.fillScreen(TFT_BLACK); expected.setTextDatum(middle_center);
  expected.setFont(&fonts::efontCN_16); expected.setTextColor(color);
  expected.drawString(text, 233, 14);
  unsigned ink = 0, missed = 0;
  for (int row = 0; row < 28; ++row) for (int x = 0; x < 468; ++x) {
    const uint16_t pixel = expected.readPixel(x, row);
    if (!pixel) continue;
    ++ink;
    if (M5.Display.readPixel(x, y - 14 + row) != pixel) ++missed;
  }
  return ink > 8 && missed == 0;
}

void openPage(Driver& d, unsigned item, Page page, const char* what) {
  d.select(item);
  d.until([&] { return d.screen().page == page; }, 4000, what);
  d.frames();
}

void chooseRegion(Driver& d, Route region) {
  d.homeMenu();
  openPage(d, 1, Page::Routes, "region menu opens");
  d.select(regionIndex(region));
  d.until([&] { return d.screen().page == Page::Home; }, 4000, "region choice returns home");
  d.check(d.save().route == region, "selected region saved");
}

bool inSpan(uint16_t species, uint16_t first, uint16_t last) { return species >= first && species <= last; }

void recentRecord(Driver& d, const char* expected, const char* shot) {
  d.frames();
  d.check(textAt(expected, 229, pokedex::ui::theme::kText), std::string("recent record shows ") + expected);
  d.shot(shot);
}
}  // namespace

// P1-16：新档切到神奥 → 只遇到神奥物种 → 捕获记录标为神奥 → 新进程恢复；
// 神奥捕完时不消耗探索步并提示换地区，切到合众后立刻遭遇合众物种。
void scenarioRegions(Driver& d, const std::string& phase) {
  if (phase == "run") {
    bootNewSave(d);
    d.check(d.save().route == Route::Kanto && sceneShows(Route::Kanto, false), "new save starts in Kanto scenery");
    d.shot("kanto-home");
    chooseRegion(d, Route::Sinnoh);
    d.check(sceneShows(Route::Sinnoh, false), "home shows Sinnoh scenery");
    d.shot("sinnoh-home");
    d.walk(500);
    const Encounter& e = d.save().encounter;
    d.check(d.save().phase == Phase::EncounterReady && e.route == Route::Sinnoh && inSpan(e.species, 387, 493),
            "walking in Sinnoh meets a Sinnoh species");
    d.event("region-encounter", "species=" + std::to_string(e.species) + " seed=" + std::to_string(e.seed));
    const uint16_t species = e.species;
    enterAndWinBattle(d);
    d.check(sceneShows(Route::Sinnoh, true), "throw page dims the Sinnoh scenery");
    for (unsigned i = 0; i < 2 && d.save().result != Result::Captured && d.screen().page == Page::Throw; ++i) {
      d.throwBall(true); d.finishCapture(); waitSettled(d, "region throw settles");
    }
    d.check(d.screen().page == Page::Result && d.save().result == Result::Captured, "Sinnoh species captured");
    d.check(d.save().firstCapture[species - 1].route == Route::Sinnoh &&
            d.save().recent[d.save().recentSize - 1].route == Route::Sinnoh, "capture records Sinnoh");
    d.check(textAt("神奥 · 时间未知", 270, pokedex::ui::theme::kMuted), "result names the Sinnoh region");
    d.shot("sinnoh-captured");
    acknowledge(d);
    d.close();
  } else if (phase == "reload") {
    d.frames();
    d.check(d.screen().page == Page::Home && d.save().route == Route::Sinnoh, "Sinnoh selection survives restart");
    d.check(sceneShows(Route::Sinnoh, false), "restart keeps Sinnoh scenery");
    const Capture& c = d.save().recent[d.save().recentSize - 1];
    d.check(d.save().recentSize == 1 && c.route == Route::Sinnoh && inSpan(c.species, 387, 493),
            "Sinnoh capture survives restart");
    d.shot("sinnoh-reload");
    d.close();
  } else if (phase == "exhausted") {
    d.frames();
    d.check(d.save().route == Route::Sinnoh && !remainingEncounters(d.save(), Route::Sinnoh),
            "fixture has every ordinary Sinnoh species");
    const uint64_t steps = d.save().totalSteps, credit = d.save().explorationCredit;
    const uint64_t spent = d.save().explorationSpent, next = d.save().nextEncounterId;
    d.walk(400);
    d.check(d.save().phase == Phase::Walking && d.save().nextEncounterId == next && d.save().explorationSpent == spent &&
            d.save().explorationCredit - credit == d.save().totalSteps - steps,
            "an exhausted region keeps every exploration step");
    d.homeMenu();
    openPage(d, 4, Page::Resources, "resources open");
    d.check(textAt("本地区已捕获完 · 可换地区", 183, pokedex::ui::theme::kGold), "exhausted region suggests switching");
    d.shot("sinnoh-exhausted");
    d.back();
    d.until([&] { return d.screen().page == Page::Menu; }, 4000, "back to menu");
    openPage(d, 1, Page::Routes, "region menu opens");
    d.select(regionIndex(Route::Unova));
    d.until([&] { return d.screen().page == Page::Home; }, 4000, "Unova choice returns home");
    const Encounter& e = d.save().encounter;
    d.check(d.save().route == Route::Unova && d.save().phase == Phase::EncounterReady && e.route == Route::Unova &&
            inSpan(e.species, 494, 649) && e.id == next && d.save().explorationSpent > spent,
            "saved credit meets a Unova species at once");
    d.check(sceneShows(Route::Unova, false), "home shows Unova scenery");
    d.shot("unova-encounter");
    d.close();
  } else if (phase == "exhausted-reload") {
    d.frames();
    d.check(d.save().route == Route::Unova && d.save().phase == Phase::EncounterReady &&
            d.save().encounter.route == Route::Unova && inSpan(d.save().encounter.species, 494, 649),
            "Unova encounter survives restart");
    d.check(!remainingEncounters(d.save(), Route::Sinnoh), "Sinnoh records survive restart");
    d.shot("unova-reload");
    d.close();
  }
}

// P1-12：HEAD 的 schema6 编码器写出的海边在途场与森林历史，升级为 schema7 关都后继续投球。
void scenarioSchema6Upgrade(Driver& d, const std::string& phase) {
  d.frames();
  d.check(d.save().schema == 7 && d.save().route == Route::Kanto, "schema6 bytes upgrade to Kanto schema7");
  d.check(d.save().firstCapture[15].route == Route::Forest && d.save().recent[0].species == 16 &&
          d.save().recent[0].route == Route::Forest, "old Forest record stays as history");
  if (phase == "schema6") {
    const Encounter& e = d.save().encounter;
    d.check(d.save().phase == Phase::ThrowReady && e.id == 2 && e.species == 72 && e.route == Route::Kanto &&
            e.seed == kThrowSeed && d.save().totalSteps == 1000 && d.save().explorationCredit == 400,
            "in-play Coast encounter moves to its Kanto region with accounts intact");
    d.check(d.screen().page == Page::Throw && sceneShows(Route::Kanto, true), "throw page shows dimmed Kanto");
    d.shot("schema6-upgraded");
    d.throwBall(true); d.finishCapture(); waitSettled(d, "upgraded throw settles");
    d.check(d.screen().page == Page::Result && d.save().result == Result::Captured &&
            d.save().encounter.attempts[0].roll == 91 && d.save().encounter.attempts[0].probability == 98,
            "frozen perfect throw captures the upgraded encounter");
    d.check(d.save().firstCapture[71].route == Route::Kanto && d.save().recentSize == 2 &&
            d.save().recent[1].route == Route::Kanto, "new capture records Kanto");
    d.check(textAt("关都 · 时间未知", 270, pokedex::ui::theme::kMuted), "result names Kanto");
    d.shot("schema6-captured");
    acknowledge(d);
    d.check(sceneShows(Route::Kanto, false), "home shows Kanto scenery");
    d.homeMenu();
    openPage(d, 4, Page::Resources, "resources open");
    openPage(d, 0, Page::Records, "records open");
    openPage(d, 1, Page::Recent, "recent captures open");
    recentRecord(d, "关都 · 累计捕捉 1 次", "schema6-recent-kanto");
    d.press(SDL_SCANCODE_RIGHT);
    recentRecord(d, "森林 · 累计捕捉 1 次", "schema6-recent-forest");
    d.close();
  } else {
    d.check(d.save().recentSize == 2 && d.save().recent[1].species == 72 && d.save().recent[1].route == Route::Kanto,
            "Kanto capture survives restart");
    d.check(d.screen().page == Page::Home && sceneShows(Route::Kanto, false), "restart shows Kanto scenery");
    d.shot("schema6-reload");
    d.close();
  }
}
}  // namespace e2e
}  // namespace simulator
