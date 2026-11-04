#include "support/progression_ui.hpp"
using namespace progression_ui;
namespace {
void peakOwnership() {
  MemoryStore store; legacyOwned(store, {1, 4, 7});
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint64_t startSteps = runtime.engine().view().totalSteps;
  walk(runtime, platform, 2); openMenu(runtime);
  tap(runtime, 233, 192); tap(runtime, 233, 238); tap(runtime, 233, 306); tap(runtime, 233, 238);
  check(runtime.engine().view().partner == 4, "switch through partner detail");
  walk(runtime, platform, 1);
  const auto& save = runtime.engine().view();
  check(save.totalSteps == startSteps + 3 && save.progress[0].walked == 2 && save.progress[3].walked == 1,
        "delayed step confirmation preserves the original carrier");
  MemoryStore triple; legacyOwned(triple, {1, 4, 7});
  Runtime switched(triple, platform); start(switched);
  openMenu(switched); walk(switched, platform, 1);
  tap(switched, 233, 192); tap(switched, 233, 238); tap(switched, 233, 306); tap(switched, 233, 238);
  walk(switched, platform, 1);
  tap(switched, 233, 374); tap(switched, 233, 374); tap(switched, 233, 238);
  check(switched.engine().view().partner == 7, "two switches precede confirmation of three peaks");
  walk(switched, platform, 1);
  check(switched.engine().view().progress[0].walked == 1 && switched.engine().view().progress[3].walked == 1 &&
        switched.engine().view().progress[6].walked == 1, "each delayed step retains its own carrier across two switches");
  MemoryStore failed; legacyOwned(failed, {1, 4});
  Runtime retry(failed, platform); start(retry);
  openMenu(retry); tap(retry, 233, 192); tap(retry, 233, 238); tap(retry, 233, 306);
  walk(retry, platform, 2); failed.fail = true; tap(retry, 233, 238);
  check(retry.screen().page == Page::Error && retry.engine().view().partner == 1, "failed switch retains the old carrier");
  walk(retry, platform, 1); failed.fail = false; select(retry, 0);
  check(retry.engine().view().partner == 4 && retry.engine().view().progress[0].walked == 3 &&
        retry.engine().view().progress[3].walked == 0, "steps during a failed switch belong to the old carrier");
  MemoryStore late;
  { Engine engine(late); engine.begin(); engine.chooseStarter(1); step(engine, 1, 7500); engine.checkpoint(); }
  Runtime evolving(late, platform); start(evolving);
  openMenu(evolving); tap(evolving, 233, 192); tap(evolving, 233, 306);
  check(evolving.screen().page == Page::EvolutionConfirm, "evolution prepared before delayed peaks");
  walk(evolving, platform, 2); tap(evolving, 233, 238); walk(evolving, platform, 1);
  check(evolving.engine().view().partner == 2 && evolving.engine().view().progress[0].walked == 7502 &&
        evolving.engine().view().progress[1].walked == 1, "late source peaks remain with the source after evolution");
}
}
int main() {
  c152::begin("PROGRESSION TEST"); peakOwnership();
  std::printf("%u step ownership Runtime checks, 0 failures\n", checks);
}
