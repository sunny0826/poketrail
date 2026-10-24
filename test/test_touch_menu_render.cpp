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
        "touching visible skip-date button upper edge opens starters, not clock or viewer");
  check(store.commits == commits && !runtime.engine().view().starter,
        "skip-date touch does not grant a starter");
  screenshot(runtime, "starter-touch-entry");
  auto& detail = M5.Touch.detail;
  detail.base_x = detail.x = 119; detail.base_y = detail.y = 228; detail.pressed = true;
  frame(runtime); idle(runtime, 160);
  check(runtime.screen().page == Page::Home && runtime.engine().view().partner == 1,
        "starter thumbnail touch grants exactly its displayed partner");
  check(runtime.screen().selected == 0, "held starter touch cannot act on next page");
  detail.pressed = false; detail.released = detail.clicked = true; frame(runtime);
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
  const int points[][2] = {{233, 0}, {233, -22}, {233, 21}, {83, 0}, {382, 0},
                          {95, -21}, {370, -21}, {95, 20}, {370, 20}};
  for (unsigned row = 0; row < 3; ++row) for (const auto& point : points) {
    MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    tap(runtime, point[0], 228 + int(row) * 48 + point[1]);
    check(runtime.screen().page == expected[row], "welcome button interior and edges activate only its label");
    check(!runtime.engine().view().starter, "welcome options never grant a partner");
  }
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  edge(runtime, false, true);
  const int outside[][2] = {{82,276}, {383,276}, {233,250}, {233,253}, {233,298},
                           {233,301}, {233,346}, {233,372}, {233,422}, {233,205}};
  const unsigned commits = store.commits;
  for (const auto& point : outside) {
    tap(runtime, point[0], point[1]);
    check(runtime.screen().page == Page::DateWelcome && runtime.screen().selected == 1,
          "welcome gaps, outer edges, unused rows and single-page footer are inert");
  }
  check(store.commits == commits, "inert welcome touches never save");
  lgfx::LGFX_Sprite label;
  label.setColorDepth(16); check(label.createSprite(300, 44), "large label canvas allocated");
  label.fillScreen(TFT_BLACK); label.setTextDatum(middle_left);
  label.setFont(&fonts::efontCN_16); label.setTextColor(theme::kGold, TFT_BLACK);
  label.drawString("暂不设置", 54, 22);
  unsigned ink = 0;
  for (int y = 0; y < 44; ++y) for (int x = 0; x < 300; ++x) if (label.readPixel(x, y)) {
    ++ink;
    check(M5.Display.readPixel(83 + x, 254 + y) == label.readPixel(x, y), "menu label uses complete left-aligned body glyphs");
  }
  check(ink > 50, "large menu label contains visible glyphs");
  label.deleteSprite();
}
void pagination() {
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  tap(runtime, 233, 228);
  check(runtime.screen().page == Page::Clock, "date first row enters clock");
  edge(runtime, false, true);
  tap(runtime, 233, 422);
  check(runtime.screen().selected == 4 && !runtime.screen().editing, "clock next page starts at item four from any row");
  tap(runtime, 233, 422);
  check(runtime.screen().selected == 8, "clock nine-item menu reaches last page");
  tap(runtime, 233, 276);
  check(runtime.screen().selected == 8 && !runtime.screen().editing, "clock final empty rows are inert");
  screenshot(runtime, "clock-last-page");
  tap(runtime, 233, 422);
  check(runtime.screen().selected == 0, "clock last page wraps to first");
  tap(runtime, 233, 422); tap(runtime, 233, 422); tap(runtime, 233, 228);
  tap(runtime, 233, 276);
  check(runtime.screen().page == Page::Starter, "clock return preserves skip-date path");
  tap(runtime, 233, 422);
  check(runtime.screen().selected == 0, "four-item starter menu has no footer action");
  tap(runtime, 233, 372);
  check(runtime.screen().page == Page::DateWelcome, "fourth starter row returns");
  tap(runtime, 233, 276); tap(runtime, 119, 324);
  check(runtime.engine().view().partner == 7, "third starter thumbnail grants Squirtle");
  openMenu(runtime); edge(runtime, false, true); tap(runtime, 233, 422);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 4, "menu flips to second page start");
  tap(runtime, 233, 372);
  check(runtime.dimmed() && runtime.screen().page == Page::Menu && runtime.screen().selected == 7,
        "menu fourth row immediately enters power saving");
  tap(runtime, 233, 228);
  check(!runtime.dimmed() && runtime.screen().page == Page::Menu, "wake touch cannot activate a menu row");
  screenshot(runtime, "menu-second-page");
  tap(runtime, 233, 422);
  check(runtime.screen().selected == 8, "menu reaches return-only third page");
  tap(runtime, 233, 276);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 8, "menu empty last-page row is inert");
  screenshot(runtime, "menu-last-page");
  tap(runtime, 233, 422);
  check(runtime.screen().selected == 0, "menu last page wraps without carrying row offset");
  tap(runtime, 233, 422); tap(runtime, 233, 422); tap(runtime, 233, 228);
  check(runtime.screen().page == Page::Home, "menu return restores exploration");
  MemoryStore four; legacyOwned(four, {1,4,7,25});
  Runtime partners(four, platform); start(partners); openMenu(partners); select(partners, 2);
  tap(partners, 233, 422);
  check(partners.screen().selected == 4, "five-item partner menu reaches back page");
  tap(partners, 233, 276);
  check(partners.screen().page == Page::Partners && partners.screen().selected == 4, "partner last empty row cannot activate");
  screenshot(partners, "partners-back-only");
  tap(partners, 233, 228);
  check(partners.screen().page == Page::Home, "partner last-page back remains reachable");
  MemoryStore full; std::vector<uint16_t> ids;
  for (unsigned id = 1; id <= 151; ++id) ids.push_back(id);
  legacyOwned(full, ids, 122);
  Runtime collection(full, platform); start(collection); openMenu(collection); select(collection, 2);
  for (unsigned page = 1; page < 38; ++page) {
    tap(collection, 233, 422);
    check(collection.screen().selected == page * 4, "touch pagination reaches every page of full collection");
  }
  screenshot(collection, "collection-last-page");
  tap(collection, 233, 372);
  check(collection.screen().page == Page::Home, "full collection last back works by touch");
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
  tap(runtime, 233, 324);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 138,
        "long evolution target belongs to selected partner");
  tap(runtime, 233, 228);
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
  const int outside[][2] = {{82,276}, {383,276}, {233,250}, {233,253}, {233,298}, {233,301}, {233,346}, {233,372}};
  const unsigned commits = store.commits;
  for (const auto& point : outside) {
    tap(runtime, point[0], point[1]);
    check(runtime.screen().page == Page::Throw && runtime.screen().selected == 0 &&
          runtime.screen().command == CommandResult::Applied, "throw-menu blank space does not act");
  }
  check(store.commits == commits, "blank throw touches cannot charge or write");
  tap(runtime, 233, 206);
  check(runtime.screen().command == CommandResult::NoBalls && !runtime.engine().throwView().active,
        "top edge of first throw row invokes only prepare");
  screenshot(runtime, "throw-no-balls");
  tap(runtime, 233, 254);
  check(runtime.screen().page == Page::Home, "top edge of second throw row keeps encounter and walks");
  openMenu(runtime); select(runtime, 0); tap(runtime, 233, 302);
  check(runtime.screen().page == Page::ReleaseConfirm, "top edge of third throw row opens separate release confirmation");
  tap(runtime, 233, 297);
  check(runtime.screen().page == Page::Throw, "bottom edge of cancel returns to throw");
  tap(runtime, 233, 324); tap(runtime, 233, 228);
  check(runtime.screen().page == Page::Result, "confirmed release opens result");
  tap(runtime, 233, 228);
  check(runtime.screen().page == Page::Result, "result has no invisible button at normal menu origin");
  screenshot(runtime, "release-result");
  tap(runtime, 233, 333);
  check(runtime.screen().page == Page::Home, "result button retains its distinct position");
}
}
int main() {
  c152::begin("TOUCH MENU TEST");
  welcomeTouch(); blankTouch(); welcomeBoundaries(); pagination(); longLabelsAndWarnings(); throwBounds();
  std::printf("%u touch menu Runtime/Button_Class/pixel checks, 0 failures\n", checks);
}
