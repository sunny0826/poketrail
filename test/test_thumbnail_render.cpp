#include "support/progression_ui.hpp"
#include "icons_generated.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
using namespace progression_ui;
namespace {
void iconAt(const data::Icon& icon, int x, int y, unsigned scale) {
  unsigned visible = 0;
  for (unsigned row = 0; row < icon.height; ++row) for (unsigned col = 0; col < icon.width; ++col) {
    const uint16_t expected = icon.pixels[row * icon.width + col];
    if (expected == kTransparentKey) continue;
    ++visible;
    for (unsigned dy = 0; dy < scale; ++dy) for (unsigned dx = 0; dx < scale; ++dx)
      check(M5.Display.readPixel(x - icon.width * scale / 2 + col * scale + dx,
                                y - icon.height * scale / 2 + row * scale + dy) == expected,
            "published thumbnail matches species RGB565 pixels at integer scale");
  }
  check(visible > 100, "thumbnail contains visible source pixels");
}
void pokemon(uint16_t id, int x, int y, unsigned scale = 1) {
  iconAt(data::kPokemonIcons[id - 1], x, y, scale);
}
void ball(uint64_t count, int y) {
  char value[32]; std::snprintf(value, sizeof(value), "%llu", static_cast<unsigned long long>(count));
  iconAt(data::kPokeBallIcon, 312, y, 1);
  check(glyph(value, y, theme::kText, 335, true), "ball inventory remains visible beside the icon");
}
void starters() {
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  select(runtime, 1);
  check(runtime.screen().page == Page::Starter, "skip date opens initial partner selection");
  const uint16_t ids[] = {1, 4, 7};
  for (unsigned i = 0; i < 3; ++i) {
    if (i) edge(runtime, false, true);
    const auto& sprite = data::kSprites[ids[i] - 1];
    std::vector<uint16_t> pixels(sprite.width * sprite.height * 4);
    check(decodeFrame(sprite, 0, 2, pixels.data(), pixels.size()), "starter animation source decodes");
    unsigned visible = 0;
    for (unsigned y = 0; y < sprite.height * 2; ++y) for (unsigned x = 0; x < sprite.width * 2; ++x) {
      const auto pixel = pixels[y * sprite.width * 2 + x];
      if (pixel == kTransparentKey) continue;
      ++visible;
      check(M5.Display.readPixel(233 - sprite.width + x, 151 - sprite.height + y) == pixel,
            "starter preview preserves original animation pixels");
    }
    check(visible > 100, "starter preview contains visible pixels");
    for (unsigned j = 0; j < 3; ++j) pokemon(ids[j], 112, 228 + j * 48);
    char name[48]; std::snprintf(name, sizeof(name), "starter-%u", ids[i]); screenshot(runtime, name);
  }
  tap(runtime, 119, 276);
  check(runtime.screen().page == Page::Home && runtime.engine().view().partner == 4,
        "touch thumbnail selects its visible starter row");
  screenshot(runtime, "home-starter");
  openMenu(runtime); ball(0, 110); screenshot(runtime, "menu-zero-balls");
}
void collection() {
  MemoryStore store; std::vector<uint16_t> ids;
  for (unsigned id = 1; id <= 151; ++id) ids.push_back(id);
  legacyOwned(store, ids, 83);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); openMenu(runtime); select(runtime, 2);
  screenshot(runtime, "partners-first");
  for (unsigned selected = 0; selected < 152; ++selected) {
    if (selected) edge(runtime, false, true);
    check(runtime.screen().selected == selected, "all collection rows reachable by right button");
    const unsigned first = selected / 4 * 4;
    for (unsigned row = first; row < first + 4 && row < 151; ++row)
      pokemon(row + 1, 112, 228 + (row - first) * 48);
    if (selected < 151) pokemon(selected + 1, 233, 137, 2);
    if (selected == 82) screenshot(runtime, "partners-carried");
    if (selected == 150) screenshot(runtime, "partners-last-species");
  }
  screenshot(runtime, "partners-last-back");
  tap(runtime, 233, 372);
  check(runtime.screen().page == Page::Home, "last-page back row remains touch reachable");
  openMenu(runtime); select(runtime, 2); tap(runtime, 119, 276);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2,
        "thumbnail row opens matching partner detail");
  pokemon(2, 233, 132, 2); pokemon(3, 112, 276);
  screenshot(runtime, "partner-before-switch");
  tap(runtime, 233, 228);
  check(runtime.engine().view().partner == 2, "detail carry action switches selected species");
  pokemon(2, 233, 132, 2); screenshot(runtime, "partner-carried");
}
void evolution() {
  MemoryStore store; legacyOwned(store, {1, 133}, 133);
  {
    Engine engine(store); check(engine.begin() == CommandResult::Applied, "Eevee fixture loads");
    step(engine, 133, 10000); engine.checkpoint();
  }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); openMenu(runtime); select(runtime, 2); select(runtime, 1);
  pokemon(133, 233, 132, 2);
  for (unsigned i = 0; i < 3; ++i) pokemon(134 + i, 112, 276 + i * 48);
  screenshot(runtime, "eevee-targets");
  tap(runtime, 119, 324);
  check(runtime.screen().page == Page::EvolutionConfirm && runtime.screen().evolution.to == 135,
        "evolution target thumbnail selects correct branch");
  pokemon(133, 164, 127, 2); pokemon(135, 302, 127, 2);
  screenshot(runtime, "evolution-captured-source");
  check(glyph("已捕获的原形态不会再遇到", 351, theme::kGold), "irreversible source exclusion explained before evolution");
  tap(runtime, 233, 276);
  check(runtime.screen().page == Page::PartnerDetail && runtime.engine().view().partner == 133,
        "cancel returns without replacing source");
}
void throwEntry() {
  MemoryStore store;
  {
    Engine engine(store); check(engine.begin() == CommandResult::Applied, "throw fixture boots");
    engine.chooseStarter(1); step(engine, 1, 5000);
    check(engine.startBattle(0) == CommandResult::Applied, "throw fixture enters battle");
    while (engine.view().phase == Phase::Battle) {
      const uint32_t t = engine.battleView().targetMs;
      engine.press(engine.battleView().expected, t);
      if (engine.view().phase == Phase::Battle) { ClockReading clock; clock.bootGeneration = engine.view().bootGeneration; engine.tick(t + 181, clock); }
    }
  }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::Throw, "saved victory opens throw entry");
  ball(5, 109);
  const auto& sprite = data::kSprites[runtime.engine().view().encounter.species - 1];
  const unsigned scale = sprite.height * 2 > 110 ? 1 : 2;
  const unsigned width = sprite.width * scale, height = sprite.height * scale;
  std::vector<uint16_t> pixels(width * height);
  check(decodeFrame(sprite, 0, scale, pixels.data(), pixels.size()), "throw preview source decodes");
  for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
    const auto pixel = pixels[y * width + x];
    if (pixel != kTransparentKey)
      check(M5.Display.readPixel(233 - width / 2 + x, 147 - height / 2 + y) == pixel,
            "throw preview retains every original opaque species pixel");
  }
  screenshot(runtime, "throw-ready");
  edge(runtime, true, false); edge(runtime, false, true);
  check(runtime.screen().page == Page::Home, "left switches and right confirms keep-walking option");
  screenshot(runtime, "home-saved-victory"); openMenu(runtime);
  ball(5, 110); screenshot(runtime, "menu-saved-victory");
  select(runtime, 0); edge(runtime, false, true);
  check(runtime.engine().throwView().active && availableBalls(runtime.engine().view()) == 5,
        "right confirms prepare without spending a ball");
  ball(5, 109); screenshot(runtime, "throw-active");
}
void exhaustedRoutes() {
  MemoryStore store; std::vector<uint16_t> captured;
  for (uint16_t id = 1; id <= 151; ++id) if (ordinaryEncounter(id) && id != 129) captured.push_back(id);
  legacyOwned(store, captured);
  Save old; check(store.load(old).status == LoadStatus::Loaded, "read exhausted fixture");
  old.captureCounts[0] = 1; old.firstCapture[0] = old.firstCapture[3]; old.firstCapture[0].species = 1;
  ++old.nextEncounterId; old.totalSteps += 500; old.undatedSteps += 500;
  check(store.commit(old) == CommitStatus::Committed, "starter previously captured in old save");
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); screenshot(runtime, "forest-exhausted");
  check(glyph("本路线已捕获完 · 可换路线", 183, theme::kGold), "exhausted route explains next action");
  select(runtime, 1); select(runtime, 1);
  check(runtime.engine().view().route == Route::Coast, "route can change after exhaustion");
  screenshot(runtime, "coast-available");
  captured.push_back(129); MemoryStore full; legacyOwned(full, captured);
  check(full.load(old).status == LoadStatus::Loaded, "read all-captured fixture");
  old.captureCounts[0] = 1; old.firstCapture[0] = old.firstCapture[3]; old.firstCapture[0].species = 1;
  ++old.nextEncounterId; old.totalSteps += 500; old.undatedSteps += 500;
  check(full.commit(old) == CommitStatus::Committed, "all ordinary species have permanent records");
  Runtime complete(full, platform); start(complete); openMenu(complete);
  screenshot(complete, "all-captured");
  check(glyph("普通宝可梦已全部捕获", 183, theme::kGold), "completed pool explained without promising impossible encounter");
}
}
int main() {
  c152::begin("THUMBNAIL TEST");
  starters(); collection(); evolution(); throwEntry(); exhaustedRoutes();
  std::printf("%u thumbnail Runtime/Button_Class/pixel checks, 0 failures\n", checks);
}
