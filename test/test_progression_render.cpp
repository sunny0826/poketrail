#include "support/progression_ui.hpp"
using namespace progression_ui;
namespace {
void evolveAndRetry() {
  MemoryStore store;
  {
    Engine engine(store); check(engine.begin() == CommandResult::Applied, "new engine boots");
    engine.chooseStarter(1); step(engine, 1, 7500); engine.checkpoint();
  }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 2); select(runtime, 0);
  check(runtime.screen().page == Page::PartnerDetail, "partner opens detail without switching");
  screenshot(runtime, "partner-ready");
  select(runtime, 1);
  check(runtime.screen().page == Page::EvolutionConfirm && owns(runtime.engine().view(), 1), "choice opens separate evolution confirmation");
  screenshot(runtime, "evolution-confirm");
  select(runtime, 1);
  check(runtime.screen().page == Page::PartnerDetail && runtime.engine().view().progress[0].spent == 0, "later keeps progress");
  select(runtime, 1);
  store.fail = store.writeThenFail = true;
  buttons(true, true); frame(runtime); frame(runtime);
  check(runtime.screen().page == Page::Error && runtime.engine().view().partner == 1, "uncertain save keeps old published partner");
  unsigned commits = store.commits; frame(runtime);
  check(store.commits == commits, "held confirmation cannot retry failed save");
  buttons(false, false); frame(runtime);
  store.fail = store.writeThenFail = false; select(runtime, 0);
  const auto& save = runtime.engine().view();
  check(runtime.screen().page == Page::Evolution && runtime.screen().evolutionAnim.to == 2,
        "successful retry plays the durably saved evolution");
  idle(runtime, kEvolutionTotalMs);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2 && runtime.screen().evolved,
        "saved evolution finishes on target detail");
  check(save.partner == 2 && save.encounter.partner == 2 && !owns(save, 1) && registered(save, 1) && owns(save, 2),
        "evolution replaces source, registers history and updates frozen encounter partner");
  check(save.progress[0].spent == 7500 && save.progress[1].walked == 0 && save.captureCounts[1] == 0,
        "evolution neither transfers steps nor fabricates captures");
  screenshot(runtime, "evolution-saved");
  select(runtime, 1); select(runtime, 2); select(runtime, 1);
  select(runtime, 3);
  check(runtime.screen().page == Page::Viewer, "history still browsable after replacing source");
  screenshot(runtime, "viewer-history");
  check(glyph("曾经获得", 390, theme::kMint), "source history label is rendered");
  edge(runtime, false, true);
  check(glyph("当前拥有", 390, theme::kMint), "evolved target ownership label is rendered");
  screenshot(runtime, "viewer-owned");
  edge(runtime, false, true);
  check(glyph("尚未获得", 390, theme::kMint), "unobtained label is rendered");
  screenshot(runtime, "viewer-unobtained");
  tap(runtime, 233, 30);
  Save loaded; check(store.load(loaded).status == LoadStatus::Loaded && loaded.progress[0].spent == 7500,
                     "evolution and charge survive production codec reload");
}
void eeveeAndCollection() {
  MemoryStore store; legacyOwned(store, {1, 133, 134, 144, 147, 151}, 133);
  {
    Engine engine(store); check(engine.begin() == CommandResult::Applied, "legacy Eevee collection migrates");
    step(engine, 133, 10001); engine.checkpoint();
  }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 2); select(runtime, 1);
  check(runtime.screen().species == 133 && menuCount(runtime.screen(), runtime.engine()) == 9, "seven branches, carry and back are pageable");
  screenshot(runtime, "eevee-branches");
  select(runtime, 1);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().command == CommandResult::TargetOwned,
        "already owned branch explains refusal without confirmation or spend");
  check(runtime.engine().view().progress[132].spent == 0, "blocked branch keeps all progress");
  select(runtime, 2); check(runtime.screen().evolution.to == 135, "second branch targets Jolteon");
  select(runtime, 0);
  check(runtime.engine().view().partner == 135 && runtime.engine().view().progress[132].walked -
        runtime.engine().view().progress[132].spent == 1, "chosen branch keeps one source step");
  check(runtime.screen().page == Page::Evolution && runtime.screen().evolutionAnim.to == 135,
        "chosen Eevee branch appears in animation");
  idle(runtime, kEvolutionTotalMs);
  select(runtime, 1); select(runtime, 3);
  check(runtime.screen().species == 144, "legacy event species remains reachable");
  select(runtime, 0); check(runtime.engine().view().partner == 144, "legacy event species may still be carried");
  screenshot(runtime, "event-partner");
  select(runtime, 1); select(runtime, 4); select(runtime, 0);
  check(runtime.engine().view().partner == 147 && runtime.engine().evolutionView(147).options[0].cost == 14500,
        "legacy Dratini may be carried and cultivated");
  screenshot(runtime, "dratini-progress");
  MemoryStore full; std::vector<uint16_t> all;
  for (unsigned id = 1; id <= kSpeciesCount; ++id) all.push_back(id);
  modernOwned(full, all);
  Runtime collection(full, platform); start(collection); openMenu(collection); select(collection, 2);
  check(menuCount(collection.screen(), collection.engine()) == 650, "all 649 owned species plus back");
  select(collection, 648); check(collection.screen().species == 649, "last Unova species is reachable using actual keys");
  screenshot(collection, "collection-last");
  select(collection, 1);
  check(collection.screen().page == Page::Partners, "last partner detail returns to the collection");
  edge(collection, true, false); screenshot(collection, "collection-last-return");
  select(collection, 649);
  check(collection.screen().page == Page::Menu, "collection final page has a working return entry");
}
void seventhEeveeBranch() {
  MemoryStore store; legacyOwned(store, {1, 133}, 133);
  { Engine engine(store); engine.begin(); step(engine, 133, 10123); engine.checkpoint(); }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 2); select(runtime, 1);
  select(runtime, 7);
  check(runtime.screen().page == Page::EvolutionConfirm && runtime.screen().evolution.to == 471, "two swipes and a key reach the seventh evolution branch");
  screenshot(runtime, "eevee-seventh-branch");
  check(runtime.screen().page == Page::EvolutionConfirm && runtime.screen().evolution.to == 471,
        "seventh branch confirms Glaceon");
  screenshot(runtime, "eevee-glaceon-confirm");
  select(runtime, 1); select(runtime, 8);
  check(runtime.screen().page == Page::Partners, "seven-branch detail final return reaches partner list");
  select(runtime, 1); select(runtime, 7); select(runtime, 0); idle(runtime, kEvolutionTotalMs);
  const auto& save = runtime.engine().view();
  check(save.partner == 471 && owns(save, 471) && !owns(save, 133) && registered(save, 133) &&
        save.progress[132].walked - save.progress[132].spent == 123 &&
        save.progress[132].spent == 10000 && save.progress[470].walked == 0,
        "seventh branch replaces source once and retains its remaining cultivation");
  screenshot(runtime, "eevee-glaceon-saved");
  Save reloaded;
  check(store.load(reloaded).status == LoadStatus::Loaded && reloaded.partner == 471 &&
        reloaded.progress[132].spent == 10000, "seventh branch persists through the real codec");
}
void peakOwnership() {
  MemoryStore store; legacyOwned(store, {1, 4, 7});
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint64_t startSteps = runtime.engine().view().totalSteps;
  walk(runtime, platform, 2);
  openMenu(runtime);
  check(runtime.engine().view().totalSteps == startSteps + 2, "two peaks wait for confirmation");
  tap(runtime, 233, 286); tap(runtime, 233, 306); tap(runtime, 233, 238);
  check(runtime.engine().view().partner == 4, "switch through partner detail");
  walk(runtime, platform, 1);
  const auto& s = runtime.engine().view();
  check(s.totalSteps == startSteps + 3 && s.progress[0].walked == 2 && s.progress[3].walked == 1,
        "actual Detector Runtime Engine preserves two old peaks across UI switch");
  MemoryStore triple; legacyOwned(triple, {1, 4, 7});
  Runtime switched(triple, platform); start(switched);
  openMenu(switched); walk(switched, platform, 1);
  tap(switched, 233, 286); tap(switched, 233, 306); tap(switched, 233, 238);
  walk(switched, platform, 1);
  tap(switched, 233, 374); tap(switched, 233, 374); tap(switched, 233, 238);
  check(switched.engine().view().partner == 7, "second switch occurs before confirmation of three peaks");
  walk(switched, platform, 1);
  check(switched.engine().view().progress[0].walked == 1 && switched.engine().view().progress[3].walked == 1 &&
        switched.engine().view().progress[6].walked == 1, "three confirmed peaks retain A B C tags across two switches");
  MemoryStore failed; legacyOwned(failed, {1, 4});
  Runtime retry(failed, platform); start(retry);
  openMenu(retry); tap(retry, 233, 286); tap(retry, 233, 306);
  walk(retry, platform, 2); failed.fail = true; tap(retry, 233, 238);
  check(retry.screen().page == Page::Error && retry.engine().view().partner == 1, "failed switch still publishes old partner");
  walk(retry, platform, 1); failed.fail = false; select(retry, 0);
  check(retry.engine().view().partner == 4 && retry.engine().view().progress[0].walked == 3 &&
        retry.engine().view().progress[3].walked == 0, "peaks sampled during failed switch keep old ownership after retry");
  MemoryStore late;
  {
    Engine e(late); e.begin(); e.chooseStarter(1); step(e, 1, 7500); e.checkpoint();
  }
  Runtime evolving(late, platform); start(evolving);
  openMenu(evolving); tap(evolving, 233, 286); tap(evolving, 233, 238); tap(evolving, 233, 306);
  check(evolving.screen().page == Page::EvolutionConfirm, "evolution prepared before delayed peaks");
  walk(evolving, platform, 2); tap(evolving, 233, 238); walk(evolving, platform, 1);
  check(evolving.engine().view().partner == 2 && evolving.engine().view().progress[0].walked == 7502 &&
        evolving.engine().view().progress[1].walked == 1, "late source peaks cultivate historical source after evolution");
}
void starterWith(MemoryStore& store, unsigned steps) {
  Engine engine(store); check(engine.begin() == CommandResult::Applied, "new engine boots");
  engine.chooseStarter(1); step(engine, 1, steps); engine.checkpoint();
}
void back(Runtime& runtime) { tap(runtime, 233, 30); }
void homeEvolveButton() {
  const auto& b = kHomeEvolveButton;
  const int bx = b.x + b.width / 2, by = b.y + b.height / 2;
  HostPlatform platform;
  MemoryStore early; starterWith(early, 7499);
  Runtime waiting(early, platform); start(waiting);
  check(!homeEvolutionReady(waiting.engine()) && glyph("携带中", 80, theme::kMint) && !glyph("进化", by, theme::kGold),
        "one step short keeps the carried label and hides the evolve button");
  tap(waiting, bx, by);
  check(waiting.screen().page == Page::Menu, "button area still opens the menu while evolution is unavailable");

  MemoryStore store; starterWith(store, 7500);
  Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::Home && glyph("进化", by, theme::kGold) && !glyph("携带中", 80, theme::kMint),
        "reaching the cost shows the evolve button on home");
  screenshot(runtime, "home-evolve-ready");
  const unsigned commits = store.commits;
  tap(runtime, bx, by);
  check(runtime.screen().page == Page::EvolutionConfirm && runtime.screen().evolution.from == 1 &&
        runtime.screen().evolution.to == 2 && store.commits == commits,
        "home button opens the existing confirmation without saving");
  select(runtime, 1);
  check(runtime.screen().page == Page::Home && runtime.engine().view().progress[0].spent == 0,
        "later from a home evolution returns home and keeps progress");
  tap(runtime, bx, by); back(runtime);
  check(runtime.screen().page == Page::Home, "top back from a home evolution returns home");
  tap(runtime, b.x + 4, by); select(runtime, 0);
  check(runtime.screen().page == Page::Evolution && runtime.screen().evolutionAnim.to == 2,
        "confirming from home plays the saved evolution");
  idle(runtime, kEvolutionTotalMs);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2 &&
        runtime.engine().view().partner == 2 && runtime.engine().view().progress[0].spent == 7500,
        "home evolution finishes on target detail with the normal charge");
  back(runtime); back(runtime); back(runtime);
  check(runtime.screen().page == Page::Home && !homeEvolutionReady(runtime.engine()) &&
        glyph("携带中", 80, theme::kMint), "new form without steps hides the button again");

  MemoryStore owned; legacyOwned(owned, {1, 2});
  { Engine e(owned); e.begin(); step(e, 1, 7500); e.checkpoint(); }
  Runtime blocked(owned, platform); start(blocked);
  check(!homeEvolutionReady(blocked.engine()) && !glyph("进化", by, theme::kGold),
        "owned target hides the button");
  MemoryStore finalForm; legacyOwned(finalForm, {1, 3}, 3);
  { Engine e(finalForm); e.begin(); step(e, 3, 20000); e.checkpoint(); }
  Runtime last(finalForm, platform); start(last);
  check(!homeEvolutionReady(last.engine()) && !glyph("进化", by, theme::kGold), "final form hides the button");

  MemoryStore eevee; legacyOwned(eevee, {1, 133, 134}, 133);
  { Engine e(eevee); e.begin(); step(e, 133, 10000); e.checkpoint(); }
  Runtime branches(eevee, platform); start(branches);
  check(glyph("进化", by, theme::kGold), "Eevee with an open branch shows one button");
  tap(branches, bx, by);
  check(branches.screen().page == Page::PartnerDetail && branches.screen().species == 133 &&
        branches.screen().selected == 2, "multiple targets open the branch list at the first open branch");
  screenshot(branches, "home-evolve-branches");
}
void legacyVictory(MemoryStore& store) {
  Save save; save.schema = 1; save.bootGeneration = 1; save.starter = save.partner = 1;
  setOwned(save, 1); save.totalSteps = save.undatedSteps = 500; save.nextEncounterId = 2;
  save.phase = Phase::ThrowReady; save.result = Result::Won;
  save.encounter.id = 1; save.encounter.species = save.encounter.partner = 1; save.encounter.ruleVersion = 1;
  for (auto& attempt : save.encounter.attempts) attempt.ruleVersion = 1;
  check(store.commit(save) == CommitStatus::Committed, "legacy victory encoded");
}
void noBallsWaitAndRelease() {
  MemoryStore store; legacyVictory(store);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::Throw && availableBalls(runtime.engine().view()) == 0,
        "migration victory restarts without retroactive balls");
  check(menuCount(runtime.screen(), runtime.engine()) == 1,
        "a legacy encounter with no balls keeps one explicit way out");
  edge(runtime, false, true);
  check(!runtime.engine().throwView().active && runtime.screen().command == CommandResult::NoBalls,
        "zero ball cannot prepare");
  screenshot(runtime, "zero-balls");
  const uint64_t encounter = runtime.engine().view().encounter.id;
  walk(runtime, platform, 1002);
  check(availableBalls(runtime.engine().view()) == 1 && runtime.engine().view().encounter.id == encounter,
        "actual samples earn one ball while preserving victory");
  check(runtime.screen().page == Page::Throw && menuCount(runtime.screen(), runtime.engine()) == 0,
        "the earned ball removes the way-out button without leaving the throw page");
  // The long walk puts the display to sleep, so the screen has to wake before
  // the press edge can reach the runtime.
  if (runtime.dimmed()) { M5.powerClick(); frame(runtime); idle(runtime, 160); frame(runtime); }
  buttons(false, false); frame(runtime);
  // The window opens on the first hold, so the key stays down from here.
  buttons(false, true); frame(runtime);
  check(runtime.engine().throwView().active && availableBalls(runtime.engine().view()) == 1,
        "a fresh press opens the window without charging inventory");
  idle(runtime, 1800);
  check(availableBalls(runtime.engine().view()) == 0 && runtime.engine().view().encounter.usedBalls == 1,
        "timeout charges one ball exactly once");
  screenshot(runtime, "legacy-throw-after-miss");
  MemoryStore restartStore; legacyVictory(restartStore);
  Runtime restart(restartStore, platform); start(restart);
  check(restart.screen().page == Page::Throw && !restart.engine().throwView().active,
        "restart restores pending victory and the aiming state, not the RAM timeline");
  check(glyph("本场没有可用精灵球", 196, theme::kRed),
        "the empty-ball page explains why the ball cannot be thrown");
  check(glyph("结束本场 · 返回探索", 238, theme::kGold, 233, false, true),
        "the empty-ball page offers one explicit way out");
  tap(restart, 233, 238);
  check(restart.engine().view().result == Result::Released && restart.screen().page == Page::Result,
        "the exit button ends the encounter and resolves it as released");
  check(restart.engine().view().ballsSpent == 0 && restart.engine().view().captureCounts[0] == 0,
        "ending an empty-ball encounter refunds nothing and grants no capture");
  screenshot(restart, "legacy-released");
}
void aimStateLayout() {
  MemoryStore store; Save save;
  save.bootGeneration = 1; save.starter = save.partner = 1; setOwned(save, 1); setRegistered(save, 1);
  save.rosterRevision = 1; save.encounterTarget = 300;
  save.totalSteps = save.undatedSteps = 5000; save.explorationSpent = 5000; save.nextEncounterId = 2;
  save.phase = Phase::ThrowReady; save.result = Result::Won;
  save.encounter.id = 1; save.encounter.species = save.encounter.partner = 1;
  check(store.commit(save) == CommitStatus::Committed, "modern throw fixture validates");
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::Throw && !runtime.engine().throwView().active &&
        menuCount(runtime.screen(), runtime.engine()) == 0,
        "the throw page opens as an aiming state with no menu");
  check(glyph("按住右键蓄力 · 松手投球", 399, theme::kMint),
        "the aiming state shows how to throw");
  check(glyph("开始蓄力才开始计时", 426, theme::kMuted),
        "the aiming state explains that the clock waits for the hold");
  check(!glyph("准备投球", 238, theme::kGold, 233, false, true) &&
        !glyph("保留遭遇 · 继续走路", 238, theme::kGold, 233, false, true) &&
        !glyph("放弃这次捕捉", 238, theme::kGold, 233, false, true),
        "the throw page carries none of the three retired menu items");
  screenshot(runtime, "throw-aiming");
}
}
int main() {
  c152::begin("PROGRESSION TEST");
  evolveAndRetry(); eeveeAndCollection(); seventhEeveeBranch(); homeEvolveButton(); peakOwnership(); noBallsWaitAndRelease(); aimStateLayout();
  std::printf("%u progression Runtime/Button_Class checks, 0 failures\n", checks);
}
