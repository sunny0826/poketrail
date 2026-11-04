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
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); openMenu(runtime); select(runtime, 2);
  edge(runtime, false, true); screenshot(runtime, "long-partner-carried");
  tap(runtime, 233, 374); tap(runtime, 233, 238);
  check(runtime.engine().view().partner == 138, "long-name fixture carries Omanyte");
  select(runtime, 1);
  check(rendered_text::contains("已拥有此形态", theme::kGold), "owned evolution target has a readable refusal");
  screenshot(runtime, "long-evolution-owned");
  MemoryStore unready; legacyOwned(unready, {1, 138}, 138);
  Runtime growing(unready, platform); start(growing); openMenu(growing); select(growing, 2); select(growing, 1); select(growing, 1);
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
int main() {
  c152::begin("MENU OUTPUT TEST"); longLabelsAndWarnings();
  std::printf("%u menu output checks, 0 failures\n", checks);
}
