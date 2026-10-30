#include "support/progression_ui.hpp"
using namespace progression_ui;
namespace {
void fresh(MemoryStore& store) {
  Engine engine(store);
  check(engine.begin() == CommandResult::Applied, "preview fixture starts a new save");
  check(engine.chooseStarter(1) == CommandResult::Applied, "preview fixture has an untrained starter");
}
void viewer(Runtime& runtime, uint16_t species) {
  if (runtime.screen().page == Page::Home) { openMenu(runtime); select(runtime, 3); }
  else if (runtime.screen().page == Page::Error) select(runtime, 1);
  else if (runtime.screen().page == Page::DateWelcome) select(runtime, 2);
  frame(runtime);
  check(runtime.screen().page == Page::Viewer, "real menu input opens the free Pokedex");
  for (unsigned i = 0; i < kSpeciesCount && !glyph(speciesName(species), 72, theme::kText, 233, false, true); ++i)
    edge(runtime, false, true);
  check(glyph(speciesName(species), 72, theme::kText, 233, false, true), "real navigation selects requested species");
}
std::vector<uint8_t> encoded(const Save& save) {
  std::vector<uint8_t> bytes(codec::maxEncodedSize); size_t size = 0;
  check(codec::encode(save, bytes.data(), bytes.size(), size), "snapshot uses production save codec");
  bytes.resize(size); return bytes;
}
std::vector<uint8_t> encoded(const Runtime& runtime) { return encoded(runtime.engine().view()); }
uint32_t preview(Runtime& runtime, uint16_t from, uint16_t to) {
  tap(runtime, 233, 423);
  check(runtime.screen().page != Page::Viewer && runtime.screen().evolutionAnim.from == from &&
        runtime.screen().evolutionAnim.to == to, "preview opens the requested evolution");
  check(glyph("进化预览", 52, theme::kText, 233, false, true), "preview is visibly distinguished from a saved evolution");
  check(evolutionFrameAt(runtime.screen().evolutionAnim, host_time_ms).stage == EvolutionStage::Gather,
        "entry touch does not skip the preview");
  return runtime.screen().evolutionAnim.startMs;
}
void unownedAndReturn() {
  MemoryStore store; fresh(store);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); viewer(runtime, 4);
  check(glyph("预览进化", 423, theme::kGold), "eligible Pokedex entry exposes preview action");
  check(!owns(runtime.engine().view(), 4), "preview species is unowned");
  screenshot(runtime, "viewer-preview-entry");
  const auto bytes = store.bytes, state = encoded(runtime); const unsigned commits = store.commits;
  tap(runtime, 330, 423);
  check(runtime.screen().page == Page::Viewer, "touch outside preview button does not activate it");
  swipe(runtime, 233, 410, 233, 435);
  check(runtime.screen().page == Page::Viewer, "drag release does not activate preview");
  swipe(runtime, 300, 200, 160, 200);
  check(glyph("火恐龙", 72, theme::kText, 233, false, true), "horizontal swipe still changes species");
  edge(runtime, true, false);
  auto& touch = M5.Touch.detail;
  touch.base_x = touch.x = 233; touch.base_y = touch.y = 423; touch.pressed = true;
  frame(runtime); touch.pressed = false; touch.released = touch.clicked = true;
  buttons(true, false); frame(runtime); touch = HostTouchDetail(); buttons(false, false); frame(runtime);
  check(runtime.screen().page == Page::Viewer && glyph("妙蛙花", 72, theme::kText, 233, false, true),
        "same-frame side key retains priority over preview touch release");
  edge(runtime, false, true);
  tap(runtime, 233, 233);
  check(platform.vibrationActive(), "central interaction still gives feedback");
  const uint32_t origin = preview(runtime, 4, 5);
  screenshot(runtime, "preview-gather", 200);
  screenshot(runtime, "preview-burst", origin + 3000 - host_time_ms);
  screenshot(runtime, "preview-reveal", origin + 3400 - host_time_ms);
  frame(runtime, origin + kEvolutionTotalMs - host_time_ms);
  check(runtime.screen().page == Page::Viewer && glyph("小火龙", 72, theme::kText, 233, false, true),
        "natural completion returns to the original Pokedex species");
  check(!runtime.screen().evolved, "preview does not claim a saved evolution");
  check(store.commits == commits && store.bytes == bytes && encoded(runtime) == state,
        "unowned preview leaves every account and saved byte unchanged");
  edge(runtime, false, true);
  check(glyph("火恐龙", 72, theme::kText, 233, false, true), "first fresh key immediately after natural return changes species");
  viewer(runtime, 6); screenshot(runtime, "viewer-final-form");
  check(!glyph("预览进化", 423, theme::kGold), "final form has no preview action");
  tap(runtime, 233, 423);
  check(runtime.screen().page == Page::Viewer, "final form footer cannot start a preview");
}
void branchesAndHeldSkip() {
  MemoryStore store; legacyOwned(store, {133, 134}, 1);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); viewer(runtime, 133);
  const auto state = encoded(runtime), bytes = store.bytes; const unsigned commits = store.commits;
  const uint16_t branches[] = {134, 135, 136, 196, 197, 470, 471};
  for (unsigned branch = 0; branch < 7; ++branch) {
    tap(runtime, 233, 423); frame(runtime);
    check(menuCount(runtime.screen(), runtime.engine()) == 8, "Eevee offers seven targets and return");
    if (!branch) screenshot(runtime, "preview-eevee-targets");
    for (unsigned i = 0; i < branch; ++i) edge(runtime, false, true);
    if (branch == 6) screenshot(runtime, "preview-eevee-seventh-target");
    buttons(true, true); frame(runtime);
    check(runtime.screen().evolutionAnim.from == 133 && runtime.screen().evolutionAnim.to == branches[branch],
          "all branches preview regardless of ownership or cultivation");
    idle(runtime, 400);
    check(evolutionFrameAt(runtime.screen().evolutionAnim, host_time_ms).stage == EvolutionStage::Gather,
          "held branch confirmation does not skip");
    buttons(false, false); frame(runtime); tap(runtime, 233, 233);
    check(evolutionFrameAt(runtime.screen().evolutionAnim, host_time_ms).stage == EvolutionStage::Reveal,
          "fresh touch skips to the evolved form");
    if (branch == 2) screenshot(runtime, "preview-eevee-flareon", 40);
    if (branch == 3) screenshot(runtime, "preview-eevee-espeon", 40);
    if (branch == 4) screenshot(runtime, "preview-eevee-umbreon", 40);
    if (branch == 5) screenshot(runtime, "preview-eevee-leafeon", 40);
    if (branch == 6) screenshot(runtime, "preview-eevee-glaceon", 40);
    frame(runtime); buttons(false, true); frame(runtime);
    check(runtime.screen().page == Page::Viewer && glyph("伊布", 72, theme::kText, 233, false, true),
          "second skip returns to the source entry without switching species");
    idle(runtime, 600);
    check(glyph("伊布", 72, theme::kText, 233, false, true), "held skip stays on original species");
    buttons(false, false); frame(runtime);
  }
  check(store.commits == commits && store.bytes == bytes && encoded(runtime) == state,
        "all Eevee previews preserve roster, progress, capture history and save");
  tap(runtime, 233, 423); frame(runtime); swipe(runtime, 300, 300, 160, 300);
  check(runtime.screen().selected == 3, "branch menu second page starts on the fourth target");
  swipe(runtime, 300, 300, 160, 300);
  check(runtime.screen().selected == 6, "branch menu third page contains the seventh target");
  edge(runtime, false, true); screenshot(runtime, "preview-eevee-return");
  select(runtime, 7); frame(runtime);
  check(runtime.screen().page == Page::Viewer, "branch menu return preserves free viewer");
  tap(runtime, 233, 423); frame(runtime); tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Viewer, "branch menu top returns to viewer");
}
void unavailableStorageAndNewSave() {
  for (unsigned fault = 0; fault < 3; ++fault) {
    const bool broken = fault != 0;
    MemoryStore store; store.fail = fault == 1;
    if (fault == 2) store.bytes = {0, 1, 2};
    HostPlatform platform; Runtime runtime(store, platform); start(runtime); viewer(runtime, 1);
    const auto bytes = store.bytes; const unsigned commits = store.commits;
    const uint32_t origin = preview(runtime, 1, 2);
    idle(runtime, 200);
    check(runtime.screen().page != Page::Error && runtime.screen().page != Page::DateWelcome,
          "preview remains available before setup or with unavailable storage");
    frame(runtime, origin + kEvolutionTotalMs - host_time_ms);
    check(runtime.screen().page == Page::Viewer && store.bytes == bytes && store.commits == commits,
          "preview returns without initializing storage or changing a new save");
    frame(runtime); tap(runtime, 233, 52);
    check(runtime.screen().page == (broken ? Page::Error : Page::DateWelcome),
          "leaving free viewer restores the pending setup or storage error");
  }
}
void continuousPlaybackAndWalking() {
  MemoryStore store; fresh(store);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); viewer(runtime, 1);
  const auto state = encoded(runtime); const uint32_t origin = preview(runtime, 1, 2);
  unsigned maxGap = 0, gap = 0, burstFrames = 0;
  while (runtime.screen().page == Page::EvolutionPreview) {
    M5.Display.beginObservation(); frame(runtime); M5.Display.endObservation();
    gap += 20;
    if (!M5.Display.publishedFrames().empty()) {
      if (gap > maxGap) maxGap = gap;
      gap = 0;
      if (host_time_ms - origin >= kEvolutionBurstMs && host_time_ms - origin < kEvolutionRevealMs)
        ++burstFrames;
    }
  }
  check(maxGap <= 40 && burstFrames >= 9, "idle preview publishes every 40ms and shows the flash stage");
  check(encoded(runtime) == state, "complete idle playback leaves the live save unchanged");
  const uint32_t walkingOrigin = preview(runtime, 1, 2);
  const uint32_t samples = runtime.metrics().samples;
  walk(runtime, platform, 6);
  check(runtime.metrics().samples >= samples + 150 && runtime.engine().view().totalSteps == 6 &&
        runtime.engine().view().progress[0].walked == 6 && runtime.engine().view().progress[1].walked == 0 &&
        runtime.engine().view().partner == 1, "preview keeps sampling and cultivation belongs to the carried source");
  frame(runtime, walkingOrigin + kEvolutionTotalMs - host_time_ms);
  check(runtime.screen().page == Page::Viewer, "walking does not interrupt preview return");
}
void everyEvolutionEdge() {
  MemoryStore store; fresh(store);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); viewer(runtime, 1);
  const auto state = encoded(runtime);
  size_t count = 0; const auto* rules = evolutionRules(count);
  for (size_t i = 0; i < count; ++i) {
    viewer(runtime, rules[i].from); tap(runtime, 233, 423); frame(runtime);
    if (runtime.screen().page == Page::EvolutionPreviewTargets) {
      const auto choices = runtime.engine().evolutionView(rules[i].from);
      unsigned selected = 0;
      while (selected < choices.count && choices.options[selected].target != rules[i].to) ++selected;
      check(selected < choices.count, "catalog target is reachable in the branch menu");
      select(runtime, selected);
    }
    check(runtime.screen().page == Page::EvolutionPreview && runtime.screen().evolutionAnim.from == rules[i].from &&
          runtime.screen().evolutionAnim.to == rules[i].to, "each catalog edge opens through real viewer input");
    const uint32_t origin = runtime.screen().evolutionAnim.startMs;
    frame(runtime, origin + kEvolutionRevealMs - host_time_ms);
    check(glyph(speciesName(rules[i].to), 379, theme::kMint, 233, false, true), "each target reveal shows its name");
    frame(runtime, origin + kEvolutionTotalMs - host_time_ms); frame(runtime);
    check(runtime.screen().page == Page::Viewer && glyph(speciesName(rules[i].from), 72, theme::kText, 233, false, true),
          "each preview returns to its source");
  }
  Save expected;
  check(codec::decode(state.data(), state.size(), expected) == codec::DecodeStatus::Ok, "initial preview snapshot decodes");
  expected.sequence = runtime.engine().view().sequence;
  check(count == 320 && encoded(runtime) == encoded(expected) && !runtime.metrics().displayErrors,
        "all 320 previews change only the normal checkpoint sequence and decode successfully");
}
void nationalViewer() {
  MemoryStore store; fresh(store);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); viewer(runtime, 1);
  edge(runtime, true, false);
  check(glyph("盖诺赛克特", 72, theme::kText, 233, false, true), "left from first entry wraps to number 649");
  screenshot(runtime, "viewer-649");
  edge(runtime, false, true);
  check(glyph("妙蛙种子", 72, theme::kText, 233, false, true), "right from number 649 wraps to first entry");
  const uint16_t boundaries[] = {252, 386, 387, 493, 494, 563, 604, 640, 648};
  for (uint16_t species : boundaries) {
    viewer(runtime, species);
    char name[48]; std::snprintf(name, sizeof(name), "viewer-%u", species);
    screenshot(runtime, name);
  }
}
void pendingEvolutionSurvivesPreview() {
  MemoryStore store; fresh(store);
  { Engine engine(store); engine.begin(); step(engine, 1, 7500); engine.checkpoint(); }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 2); select(runtime, 0); select(runtime, 1);
  store.fail = true; select(runtime, 0);
  check(runtime.screen().page == Page::Error && runtime.engine().savePending(), "real evolution awaits save retry");
  viewer(runtime, 133); tap(runtime, 233, 423); frame(runtime); select(runtime, 2);
  const uint32_t origin = runtime.screen().evolutionAnim.startMs;
  frame(runtime, origin + kEvolutionTotalMs - host_time_ms); frame(runtime); tap(runtime, 233, 52);
  check(runtime.screen().page == Page::Error, "preview preserves pending save error");
  store.fail = false; frame(runtime); select(runtime, 0);
  check(runtime.screen().page == Page::Evolution && runtime.screen().evolutionAnim.from == 1 &&
        runtime.screen().evolutionAnim.to == 2 && runtime.engine().view().progress[0].spent == 7500,
        "retry after a different preview plays and charges the original evolution once");
}
}
int main() {
  c152::begin("EVOLUTION PREVIEW TEST");
  unownedAndReturn(); branchesAndHeldSkip(); unavailableStorageAndNewSave();
  continuousPlaybackAndWalking(); everyEvolutionEdge(); nationalViewer(); pendingEvolutionSurvivesPreview();
  std::printf("%u evolution preview Runtime/Button_Class/M5GFX checks, 0 failures\n", checks);
}
