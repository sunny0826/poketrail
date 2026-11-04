#include "support/progression_ui.hpp"
#include "support/rendered_text.hpp"
using namespace progression_ui;
namespace {
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
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); openMenu(runtime);
  select(runtime, MenuItem::Partner); select(runtime, 0);
  edge(runtime, false, true); screenshot(runtime, "long-partner-carried");
  tap(runtime, 233, 374); tap(runtime, 233, 238);
  check(runtime.engine().view().partner == 138, "long-name fixture carries Omanyte");
  select(runtime, 1);
  check(rendered_text::contains("已拥有此形态", theme::kGold), "owned evolution target has a readable refusal");
  screenshot(runtime, "long-evolution-owned");
  MemoryStore unready; legacyOwned(unready, {1, 138}, 138);
  Runtime growing(unready, platform); start(growing); openMenu(growing); select(growing, MenuItem::Partner); select(growing, 1);
  check(rendered_text::contains("培养步数还不够", theme::kGold), "unavailable evolution explains the missing progress");
  screenshot(growing, "long-evolution-unready");
  DegradedStore backup; legacyOwned(backup, {1});
  Runtime recovered(backup, platform); start(recovered);
  check(rendered_text::contains("已从备用存档恢复", theme::kGold), "recovery warning is readable on exploration");
  screenshot(recovered, "home-recovery-warning");
  Save saved; check(store.load(saved).status == LoadStatus::Loaded, "step-warning fixture loads saved state");
  saved.stepsIncomplete = true; saved.today.complete = false;
  check(store.commit(saved) == CommitStatus::Committed, "step-warning fixture validates");
  Runtime incomplete(store, platform); start(incomplete);
  check(rendered_text::contains("部分步数缺失", theme::kRed), "step warning is readable on exploration");
  screenshot(incomplete, "home-step-warning");
}
}
// The banner is only a target while an encounter waits; otherwise keys and touch pass over it.
void mainMenuHub() {
  MemoryStore walking; legacyOwned(walking, {1, 4});
  HostPlatform platform; Runtime runtime(walking, platform); start(runtime); openMenu(runtime);
  screenshot(runtime, "menu-hub-walking");
  edge(runtime, true, false);
  check(runtime.screen().selected == unsigned(MenuItem::Explore), "left from the first tile wraps past the walking banner");
  edge(runtime, false, true);
  check(runtime.screen().selected == unsigned(MenuItem::Region), "right from the exit wraps past the walking banner");
  tap(runtime, 233, 104);
  check(runtime.screen().page == Page::Menu, "walking banner ignores touch");
  tap(runtime, 107, 296);
  check(runtime.screen().page == Page::Records && runtime.screen().selected == 0, "records tile opens the records hub");
  screenshot(runtime, "records-hub");
  tap(runtime, 233, 389);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == unsigned(MenuItem::Records),
        "records exit pill returns focus to its tile");
  tap(runtime, 359, 296);
  check(runtime.screen().page == Page::Settings, "settings tile opens settings");
  screenshot(runtime, "settings-page");

  MemoryStore waiting;
  { Engine e(waiting); e.begin(); e.chooseStarter(1); step(e, 1, 5000); e.checkpoint(); }
  Runtime encounter(waiting, platform); start(encounter); openMenu(encounter);
  check(rendered_text::contains("继续遭遇", theme::kGold, 78, 130), "waiting encounter banner names the resume action");
  screenshot(encounter, "menu-hub-encounter");
  edge(encounter, true, false);
  check(encounter.screen().selected == unsigned(MenuItem::Explore), "left from a waiting banner reaches the exit");
  edge(encounter, false, true);
  check(encounter.screen().selected == unsigned(MenuItem::Encounter), "waiting banner joins key navigation");
  tap(encounter, 233, 104);
  check(encounter.screen().page == Page::Encounter, "touching the waiting banner opens the encounter");
}
int main() {
  c152::begin("MENU OUTPUT TEST"); longLabelsAndWarnings(); mainMenuHub();
  std::printf("%u menu output checks, 0 failures\n", checks);
}
