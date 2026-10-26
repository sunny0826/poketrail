#include "support/progression_ui.hpp"
using namespace progression_ui;
namespace {
int selectedTop() {
  for (int y = 210; y < 395; ++y)
    if (M5.Display.readPixel(200, y) == theme::kPanelActive) return y;
  check(false, "selected menu button has a visible surface");
  return 0;
}
void welcomeTouch() {
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::DateWelcome, "fresh save opens date welcome");
  edge(runtime, false, true);
  screenshot(runtime, "date-skip-selected");
  const int top = selectedTop();
  const unsigned commits = store.commits;
  tap(runtime, 233, top + 1);
  check(runtime.screen().page == Page::Starter,
        "releasing the visible skip-date button opens starters, not clock or viewer");
  check(store.commits == commits && !runtime.engine().view().starter,
        "skip-date touch does not grant a starter");
  screenshot(runtime, "starter-touch-entry");
  auto& detail = M5.Touch.detail;
  detail.base_x = detail.x = 119; detail.base_y = detail.y = 238; detail.pressed = true;
  frame(runtime); idle(runtime, 160);
  check(runtime.screen().page == Page::Starter && !runtime.engine().view().starter,
        "holding a starter button waits for release without granting a partner");
  detail.pressed = false; detail.released = detail.clicked = true; frame(runtime);
  check(runtime.screen().page == Page::Home && runtime.engine().view().partner == 1,
        "starter click release grants exactly its displayed partner once");
  check(runtime.screen().selected == 0, "starter release cannot act on next page");
  detail = HostTouchDetail(); frame(runtime);
  check(availableBalls(runtime.engine().view()) == 0, "touch onboarding preserves zero-ball new save");
  screenshot(runtime, "home-touch-onboarding");
  Runtime restart(store, platform); start(restart);
  check(restart.screen().page == Page::Home && restart.engine().view().partner == 1,
        "starter chosen by touch survives codec save and runtime restart");
}
void blankTouch() {
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  edge(runtime, false, true);
  const unsigned commits = store.commits;
  tap(runtime, 20, selectedTop() + 10);
  check(runtime.screen().page == Page::DateWelcome && runtime.screen().selected == 1,
        "touch outside visible menu buttons cannot activate a row");
  check(store.commits == commits, "blank touch cannot write a save");
}
void welcomeBoundaries() {
  const Page expected[] = {Page::Clock, Page::Starter, Page::Viewer};
  const int points[][2] = {{233, 0}, {233, -30}, {233, 29}, {73, 0}, {392, 0},
                          {88, -29}, {377, -29}, {88, 28}, {377, 28}};
  for (unsigned row = 0; row < 3; ++row) for (const auto& point : points) {
    MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    tap(runtime, point[0], menu_layout::center(row) + point[1]);
    check(runtime.screen().page == expected[row], "large welcome button interior and edges activate only its label");
    check(!runtime.engine().view().starter, "welcome options never grant a partner");
  }
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  edge(runtime, false, true);
  const int outside[][2] = {{72,306}, {393,306}, {233,268}, {233,275}, {233,336},
                           {233,343}, {233,404}, {233,430}, {233,207}};
  const unsigned commits = store.commits;
  for (const auto& point : outside) {
    tap(runtime, point[0], point[1]);
    check(runtime.screen().page == Page::DateWelcome && runtime.screen().selected == 1,
          "welcome gaps, outer edges and single-page footer are inert");
  }
  check(store.commits == commits, "inert welcome touches never save");
  lgfx::LGFX_Sprite label;
  label.setColorDepth(16); check(label.createSprite(320, 60), "large label canvas allocated");
  label.fillScreen(TFT_BLACK); label.setTextDatum(middle_center);
  label.setFont(&fonts::efontCN_24); label.setTextColor(theme::kGold, TFT_BLACK);
  label.drawString("暂不设置", 160, 30);
  unsigned ink = 0;
  for (int y = 0; y < 60; ++y) for (int x = 0; x < 320; ++x) if (label.readPixel(x, y)) {
    ++ink;
    check(M5.Display.readPixel(73 + x, 276 + y) == label.readPixel(x, y), "menu label uses complete centered 24px glyphs");
  }
  check(ink > 50, "large menu label contains visible glyphs");
  label.deleteSprite();
}
void pagination() {
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  tap(runtime, 233, 238);
  check(runtime.screen().page == Page::Clock, "date first row enters clock on release");
  edge(runtime, false, true);
  tap(runtime, 298, 430);
  check(runtime.screen().selected == 3 && !runtime.screen().editing, "clock next button starts at item three from any row");
  tap(runtime, 298, 430);
  check(runtime.screen().selected == 6, "clock nine-item menu reaches last page");
  screenshot(runtime, "clock-last-page");
  tap(runtime, 298, 430);
  check(runtime.screen().selected == 0, "clock next button wraps to first");
  tap(runtime, 168, 430);
  check(runtime.screen().selected == 6, "clock previous button wraps to last");
  tap(runtime, 233, 374);
  check(runtime.screen().page == Page::DateWelcome, "clock last-row back preserves first-run path");
  tap(runtime, 233, 306);
  check(runtime.screen().page == Page::Starter, "clock return preserves skip-date path");
  tap(runtime, 298, 430);
  check(runtime.screen().selected == 3, "four-item starter menu paginates to its return row");
  tap(runtime, 233, 238);
  check(runtime.screen().page == Page::DateWelcome, "fourth starter row returns from second page");
  tap(runtime, 233, 306); tap(runtime, 119, 374);
  check(runtime.engine().view().partner == 7, "third starter thumbnail grants Squirtle on release");
  openMenu(runtime); edge(runtime, false, true); tap(runtime, 298, 430);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3, "menu flips to second page start");
  screenshot(runtime, "menu-second-page");
  tap(runtime, 298, 430);
  check(runtime.screen().selected == 6, "menu reaches third page");
  tap(runtime, 233, 150);
  check(runtime.dimmed() && runtime.screen().page == Page::Menu && runtime.screen().selected == 6,
        "menu sleep button acts once on release");
  tap(runtime, 233, 150);
  check(!runtime.dimmed() && runtime.screen().page == Page::Menu && runtime.screen().selected == 6,
        "wake touch cannot activate a menu row");
  screenshot(runtime, "menu-last-page");
  tap(runtime, 298, 430);
  check(runtime.screen().selected == 0, "menu last page wraps without carrying row offset");
  tap(runtime, 168, 430);
  check(runtime.screen().selected == 6, "menu previous button wraps from first to last");
  tap(runtime, 298, 430);
  swipe(runtime, 233, 150, 120, 150);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3,
        "left swipe starting on a button only turns the page");
  swipe(runtime, 233, 218, 380, 218);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 0,
        "right swipe returns without activating its start button");
  tap(runtime, 298, 430); tap(runtime, 298, 430); tap(runtime, 233, 286);
  check(runtime.screen().page == Page::Home, "menu return restores exploration");
  openMenu(runtime);
  tap(runtime, 298, 430); tap(runtime, 298, 430); tap(runtime, 233, 218);
  check(runtime.screen().page == Page::Ruins, "third-page touch row opens the ruins");
  frame(runtime);
  tap(runtime, 233, 30);
  check(runtime.screen().page == Page::Menu, "ruins top touch returns to the menu");
  MemoryStore four; legacyOwned(four, {1,4,7,25});
  Runtime partners(four, platform); start(partners); openMenu(partners); select(partners, 2);
  tap(partners, 298, 430);
  check(partners.screen().selected == 3, "five-item partner menu reaches second page");
  tap(partners, 233, 374);
  check(partners.screen().page == Page::Partners && partners.screen().selected == 3,
        "partner last empty row cannot activate");
  screenshot(partners, "partners-back-page");
  tap(partners, 233, 306);
  check(partners.screen().page == Page::Menu, "partner back returns to the main menu");
  MemoryStore full; std::vector<uint16_t> ids;
  for (unsigned id = 1; id <= 251; ++id) ids.push_back(id);
  modernOwned(full, ids, 122);
  Runtime collection(full, platform); start(collection); openMenu(collection); select(collection, 2);
  for (unsigned page = 1; page < 84; ++page) {
    tap(collection, 298, 430);
    check(collection.screen().selected == page * 3, "touch pagination reaches every page of full collection");
  }
  screenshot(collection, "collection-last-page");
  tap(collection, 233, 374);
  check(collection.screen().page == Page::Menu, "full collection last back works by touch");
}
void resourcesAndHomeTouch() {
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  tap(runtime, 233, 306); tap(runtime, 233, 238);
  check(runtime.screen().page == Page::Home, "touch onboarding reaches exploration");
  tap(runtime, 233, 233);
  check(runtime.screen().page == Page::Menu, "home touch opens the main menu without a side key");
  screenshot(runtime, "menu-first-page");
  check(M5.Display.readPixel(200, 150) == theme::kPanelActive,
        "main menu uses the space freed by moving resources out");
  tap(runtime, 233, 218);
  check(runtime.screen().page == Page::Routes, "moved second menu button keeps its touch target");
  tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Menu, "routes top touch returns to the moved main menu");
  tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Home, "main-menu top touch returns home");
  tap(runtime, 233, 233); tap(runtime, 298, 430); tap(runtime, 233, 218);
  check(runtime.screen().page == Page::Resources, "resources entry opens its own page");
  screenshot(runtime, "resources");
  tap(runtime, 233, 306);
  check(runtime.screen().page == Page::Clock, "resources opens clock settings");
  tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Resources, "clock top touch returns to resources");
  tap(runtime, 233, 238);
  check(runtime.screen().page == Page::Records, "resources opens record catalog");
  screenshot(runtime, "records-catalog");
  tap(runtime, 233, 238);
  check(runtime.screen().page == Page::FirstCaptures, "record catalog opens first captures");
  swipe(runtime, 233, 238, 120, 238);
  check(runtime.screen().page == Page::FirstCaptures && runtime.screen().selected == 1,
        "record swipe advances exactly one entry");
  tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Records, "record top touch returns to record catalog");
  tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Resources, "record catalog top touch returns to resources");
  tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Menu, "resources top touch returns to main menu");
  tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Home, "complete touch back chain reaches exploration");
}
class DegradedStore : public MemoryStore {
 public:
  LoadResult load(Save& save) override {
    auto result = MemoryStore::load(save);
    if (result.status == LoadStatus::Loaded) result.degraded = true;
    return result;
  }
};
void longLabelsAndWarnings() {
  MemoryStore store; legacyOwned(store, {1, 122, 138, 139}, 122);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); openMenu(runtime); select(runtime, 2);
  edge(runtime, false, true); screenshot(runtime, "long-partner-carried");
  tap(runtime, 233, 374);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 138,
        "long evolution target belongs to selected partner");
  tap(runtime, 233, 238);
  check(runtime.engine().view().partner == 138 &&
        runtime.engine().evolutionView(138).options[0].status == CommandResult::TargetOwned,
        "long owned-target label follows actual carry action");
  screenshot(runtime, "long-evolution-owned");
  MemoryStore unready; legacyOwned(unready, {1, 138}, 138);
  Runtime growing(unready, platform); start(growing); openMenu(growing); select(growing, 2); select(growing, 1);
  check(growing.engine().evolutionView(138).options[0].status == CommandResult::InsufficientProgress,
        "long unavailable-target label has insufficient cultivation");
  screenshot(growing, "long-evolution-unready");
  DegradedStore backup; legacyOwned(backup, {1});
  Runtime recovered(backup, platform); start(recovered);
  check(glyph("已从备用存档恢复", 428, theme::kGold), "recovery warning remains visible on exploration");
  screenshot(recovered, "home-recovery-warning");
  Save saved;
  check(store.load(saved).status == LoadStatus::Loaded, "step-warning fixture loads saved state");
  saved.stepsIncomplete = true; saved.today.complete = false;
  check(store.commit(saved) == CommitStatus::Committed, "step-warning fixture uses valid persisted flag");
  Runtime incomplete(store, platform); start(incomplete);
  check(glyph("部分步数缺失", 428, theme::kRed), "step warning remains visible on exploration");
  screenshot(incomplete, "home-step-warning");
}
void throwBounds() {
  MemoryStore store; Save save;
  save.schema = 1; save.bootGeneration = 1; save.starter = save.partner = 1;
  setOwned(save, 1); save.totalSteps = save.undatedSteps = 500; save.nextEncounterId = 2;
  save.phase = Phase::ThrowReady; save.result = Result::Won;
  save.encounter.id = 1; save.encounter.species = save.encounter.partner = 1; save.encounter.ruleVersion = 1;
  for (auto& attempt : save.encounter.attempts) attempt.ruleVersion = 1;
  check(store.commit(save) == CommitStatus::Committed, "zero-ball saved victory validates");
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const int outside[][2] = {{72,306}, {393,306}, {233,268}, {233,275}, {233,336},
                           {233,343}, {233,404}, {233,430}};
  const unsigned commits = store.commits;
  for (const auto& point : outside) {
    tap(runtime, point[0], point[1]);
    check(runtime.screen().page == Page::Throw && runtime.screen().selected == 0 &&
          runtime.screen().command == CommandResult::Applied, "throw-menu blank space does not act");
  }
  check(store.commits == commits, "blank throw touches cannot charge or write");
  tap(runtime, 233, 208);
  check(runtime.screen().command == CommandResult::NoBalls && !runtime.engine().throwView().active,
        "top edge of first throw row invokes only prepare on release");
  screenshot(runtime, "throw-no-balls");
  tap(runtime, 233, 276);
  check(runtime.screen().page == Page::Home, "top edge of second throw row keeps encounter and walks");
  openMenu(runtime); select(runtime, 0); tap(runtime, 233, 344);
  check(runtime.screen().page == Page::ReleaseConfirm, "top edge of third throw row opens separate release confirmation");
  tap(runtime, 233, 335);
  check(runtime.screen().page == Page::Throw, "bottom edge of cancel returns to throw");
  tap(runtime, 233, 374); tap(runtime, 233, 238);
  check(runtime.screen().page == Page::Result, "confirmed release opens result");
  tap(runtime, 233, 238);
  check(runtime.screen().page == Page::Result, "result has no invisible button at normal menu origin");
  screenshot(runtime, "release-result");
  tap(runtime, 233, 355);
  check(runtime.screen().page == Page::Home, "result button retains its distinct position");
}
}
int main() {
  c152::begin("TOUCH MENU TEST");
  welcomeTouch(); blankTouch(); welcomeBoundaries(); pagination(); resourcesAndHomeTouch();
  longLabelsAndWarnings(); throwBounds();
  std::printf("%u touch menu Runtime/Button_Class/pixel checks, 0 failures\n", checks);
}
