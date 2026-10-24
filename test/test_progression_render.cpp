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
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2 && runtime.screen().evolved,
        "retry returns to durably evolved detail");
  check(save.partner == 2 && save.encounter.partner == 2 && !owns(save, 1) && registered(save, 1) && owns(save, 2),
        "evolution replaces source, registers history and updates frozen encounter partner");
  check(save.progress[0].spent == 7500 && save.progress[1].walked == 0 && save.captureCounts[1] == 0,
        "evolution neither transfers steps nor fabricates captures");
  screenshot(runtime, "evolution-saved");
  select(runtime, 2); select(runtime, 1); select(runtime, 3);
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
  check(runtime.screen().species == 133 && menuCount(runtime.screen(), runtime.engine()) == 5, "three branches and back are pageable");
  screenshot(runtime, "eevee-branches");
  select(runtime, 1);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().command == CommandResult::TargetOwned,
        "already owned branch explains refusal without confirmation or spend");
  check(runtime.engine().view().progress[132].spent == 0, "blocked branch keeps all progress");
  select(runtime, 2); check(runtime.screen().evolution.to == 135, "second branch targets Jolteon");
  select(runtime, 0);
  check(runtime.engine().view().partner == 135 && runtime.engine().view().progress[132].walked -
        runtime.engine().view().progress[132].spent == 1, "chosen branch keeps one source step");
  select(runtime, 1); select(runtime, 3);
  check(runtime.screen().species == 144, "legacy event species remains reachable");
  select(runtime, 0); check(runtime.engine().view().partner == 144, "legacy event species may still be carried");
  screenshot(runtime, "event-partner");
  select(runtime, 1); select(runtime, 4); select(runtime, 0);
  check(runtime.engine().view().partner == 147 && runtime.engine().evolutionView(147).options[0].cost == 14500,
        "legacy Dratini may be carried and cultivated");
  screenshot(runtime, "dratini-progress");
  MemoryStore full; std::vector<uint16_t> all;
  for (unsigned id = 1; id <= 151; ++id) all.push_back(id);
  legacyOwned(full, all);
  Runtime collection(full, platform); start(collection); openMenu(collection); select(collection, 2);
  check(menuCount(collection.screen(), collection.engine()) == 152, "all 151 owned species plus back");
  select(collection, 150); check(collection.screen().species == 151, "last owned species is reachable using actual keys");
  screenshot(collection, "collection-last");
}
void peakOwnership() {
  MemoryStore store; legacyOwned(store, {1, 4, 7});
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint64_t startSteps = runtime.engine().view().totalSteps;
  openMenu(runtime); walk(runtime, platform, 2);
  check(runtime.engine().view().totalSteps == startSteps, "two peaks wait for confirmation");
  tap(runtime, 233, 374); tap(runtime, 233, 306); tap(runtime, 233, 238);
  check(runtime.engine().view().partner == 4, "switch through partner detail");
  walk(runtime, platform, 1);
  const auto& s = runtime.engine().view();
  check(s.totalSteps == startSteps + 3 && s.progress[0].walked == 2 && s.progress[3].walked == 1,
        "actual Detector Runtime Engine preserves two old peaks across UI switch");
  MemoryStore triple; legacyOwned(triple, {1, 4, 7});
  Runtime switched(triple, platform); start(switched);
  openMenu(switched); walk(switched, platform, 1);
  tap(switched, 233, 374); tap(switched, 233, 306); tap(switched, 233, 238);
  walk(switched, platform, 1);
  tap(switched, 233, 374); tap(switched, 233, 374); tap(switched, 233, 238);
  check(switched.engine().view().partner == 7, "second switch occurs before confirmation of three peaks");
  walk(switched, platform, 1);
  check(switched.engine().view().progress[0].walked == 1 && switched.engine().view().progress[3].walked == 1 &&
        switched.engine().view().progress[6].walked == 1, "three confirmed peaks retain A B C tags across two switches");
  MemoryStore failed; legacyOwned(failed, {1, 4});
  Runtime retry(failed, platform); start(retry);
  openMenu(retry); tap(retry, 233, 374); tap(retry, 233, 306);
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
  openMenu(evolving); tap(evolving, 233, 374); tap(evolving, 233, 238); tap(evolving, 233, 306);
  check(evolving.screen().page == Page::EvolutionConfirm, "evolution prepared before delayed peaks");
  walk(evolving, platform, 2); tap(evolving, 233, 238); walk(evolving, platform, 1);
  check(evolving.engine().view().partner == 2 && evolving.engine().view().progress[0].walked == 7502 &&
        evolving.engine().view().progress[1].walked == 1, "late source peaks cultivate historical source after evolution");
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
  edge(runtime, false, true);
  check(!runtime.engine().throwView().active && runtime.screen().command == CommandResult::NoBalls, "zero ball cannot prepare");
  screenshot(runtime, "zero-balls");
  tap(runtime, 233, 306); idle(runtime, 1200);
  check(runtime.screen().page == Page::Home && runtime.engine().view().phase == Phase::ThrowReady,
        "waiting remains on home across syncPage ticks");
  const uint64_t encounter = runtime.engine().view().encounter.id;
  walk(runtime, platform, 1002);
  check(availableBalls(runtime.engine().view()) == 1 && runtime.engine().view().encounter.id == encounter,
        "actual samples earn one ball while preserving victory");
  if (runtime.dimmed()) edge(runtime, false, true);
  openMenu(runtime); select(runtime, 0); check(runtime.screen().page == Page::Throw, "menu explicitly resumes same throw");
  edge(runtime, false, true);
  check(runtime.engine().throwView().active && availableBalls(runtime.engine().view()) == 1, "prepare does not charge inventory");
  idle(runtime, 1800);
  check(availableBalls(runtime.engine().view()) == 0 && runtime.engine().view().encounter.usedBalls == 1,
        "timeout charges one ball exactly once");
  tap(runtime, 233, 374);
  check(runtime.screen().page == Page::ReleaseConfirm, "release requires separate confirmation");
  screenshot(runtime, "release-confirm");
  tap(runtime, 233, 306); check(runtime.screen().page == Page::Throw, "release cancel preserves encounter");
  tap(runtime, 233, 374); tap(runtime, 233, 238);
  check(runtime.engine().view().result == Result::Released && runtime.screen().page == Page::Result,
        "release persists distinct resolved result");
  check(runtime.engine().view().ballsSpent == 1 && runtime.engine().view().captureCounts[0] == 0,
        "release neither refunds ball nor grants capture");
  screenshot(runtime, "released");
  MemoryStore restartStore; legacyVictory(restartStore);
  Runtime waiting(restartStore, platform); start(waiting); tap(waiting, 233, 306);
  Runtime restart(restartStore, platform); start(restart);
  check(restart.screen().page == Page::Throw && !restart.engine().throwView().active,
        "restart restores pending victory and explicit wait option, not RAM timeline");
}
}
int main() {
  c152::begin("PROGRESSION TEST");
  evolveAndRetry(); eeveeAndCollection(); peakOwnership(); noBallsWaitAndRelease();
  std::printf("%u progression Runtime/Button_Class checks, 0 failures\n", checks);
}
