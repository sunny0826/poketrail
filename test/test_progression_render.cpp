#include "support/progression_ui.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
#include <zlib.h>
using namespace progression_ui;
namespace {
void savePng(const char* name) {
  constexpr int N = 468; std::vector<uint8_t> raw(N * (1 + N * 3), 0);
  for (int i = 0; i < N * N; ++i) {
    const uint16_t p = M5.Display.readPixel(i % N, i / N);
    uint8_t* q = &raw[(i / N) * (1 + N * 3) + 1 + (i % N) * 3];
    q[0] = (p >> 11) * 255 / 31; q[1] = ((p >> 5) & 63) * 255 / 63; q[2] = (p & 31) * 255 / 31;
  }
  uLongf zl = compressBound(uLong(raw.size())); std::vector<uint8_t> z(zl);
  check(compress2(z.data(), &zl, raw.data(), uLong(raw.size()), Z_BEST_SPEED) == Z_OK, "png");
  FILE* f = std::fopen(verificationOutput((std::string(name) + ".png").c_str()).c_str(), "wb");
  check(f != nullptr, "open png");
  auto w = [&](const char* t, const uint8_t* d, size_t n) {
    uint8_t b[4] = {uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)};
    std::fwrite(b,1,4,f); std::fwrite(t,1,4,f);
    if (n) std::fwrite(d,1,n,f);
    uLong c = crc32(0, reinterpret_cast<const Bytef*>(t), 4);
    if (n) c = crc32(c, d, uInt(n));
    b[0]=c>>24; b[1]=c>>16; b[2]=c>>8; b[3]=c; std::fwrite(b,1,4,f);
  };
  static const uint8_t hdr[13] = {0,0,1,212,0,0,1,212,8,2,0,0,0};
  std::fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
  w("IHDR", hdr, 13); w("IDAT", z.data(), zl); w("IEND", nullptr, 0); std::fclose(f);
}
void checkCircleBounds(const char* label) {
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x) {
    const int dx = x - 233, dy = y - 233;
    if (dx * dx + dy * dy > 228 * 228 && M5.Display.readPixel(x, y) != TFT_BLACK) check(false, label);
  }
}
bool hasViewerBackIcon() {
  for (int dx = -7; dx <= 7; ++dx)
    if (M5.Display.readPixel(112 + dx, 52) != theme::kMuted) return false;
  return M5.Display.readPixel(111, 46) == theme::kMuted &&
         M5.Display.readPixel(111, 58) == theme::kMuted;
}
void ownedSave649(MemoryStore& store, std::initializer_list<uint16_t> ids,
                  uint16_t recentSpecies = 0, uint64_t eeveeSteps = 0) {
  Save s;
  s.route = Route::Kanto;
  s.sequence = 1; s.bootGeneration = 1; s.encounterTarget = 300;
  s.starter = s.partner = 1; s.rosterRevision = 1;
  setRegistered(s, 1); setOwned(s, 1);
  unsigned captures = 0;
  for (uint16_t id : ids) {
    setRegistered(s, id); setOwned(s, id);
    if (id == 1) continue;
    s.captureCounts[id - 1] = 1;
    Capture& c = s.firstCapture[id - 1];
    c.species = id; c.route = Route::Kanto;
    c.at.bootGeneration = 1; c.at.totalSteps = ++captures * 250;
  }
  s.explorationSpent = captures * 250;
  s.explorationCredit = 500 + eeveeSteps;
  s.totalSteps = s.undatedSteps = s.explorationSpent + s.explorationCredit;
  if (eeveeSteps) s.progress[132].walked = eeveeSteps;
  const uint64_t thresholds[] = {10000, 50000, 100000};
  for (unsigned i = 0; i < 3; ++i) if (s.totalSteps >= thresholds[i]) {
    s.lifetime[i].reached = true; s.lifetime[i].first.bootGeneration = 1;
    s.lifetime[i].first.totalSteps = thresholds[i];
  }
  s.nextEncounterId = captures + 1;
  if (recentSpecies >= 1 && recentSpecies <= kSpeciesCount) {
    s.recentSize = 1;
    s.recent[0] = s.firstCapture[recentSpecies - 1];
  }
  check(validate(s), "649 fixture validates");
  check(store.commit(s) == CommitStatus::Committed, "649 fixture commits");
}
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

void partnersReturnPosition() {
  MemoryStore store;
  ownedSave649(store, {1, 4, 25, 133, 439, 649}, 0, 10000);
  HostPlatform platform; Runtime r(store, platform); start(r);
  openMenu(r); select(r, MenuItem::Partner); select(r, 0);
  check(r.screen().page == Page::Partners && r.screen().selected == 0, "fresh switch partner starts at row 0");

  tap(r, 298, 430);
  check(r.screen().selected == 3, "page 2 starts at row 3");
  tap(r, 233, 306);
  check(r.screen().page == Page::PartnerDetail && r.screen().species == 439, "page 2 row 1 opens 439");
  tap(r, 112, 52);
  check(r.screen().page == Page::Partners && r.screen().selected == 4, "top back preserves page 2 selection 439");

  tap(r, 233, 374);
  check(r.screen().page == Page::PartnerDetail && r.screen().species == 649, "page 2 row 2 opens 649");
  tap(r, 233, 306);
  check(r.screen().page == Page::Partners && r.screen().selected == 5, "detail return item preserves page 2 selection 649");

  tap(r, 112, 52);
  check(r.screen().page == Page::CarriedPartner, "back from partners returns to carried partner");
  select(r, 0);
  check(r.screen().page == Page::Partners && r.screen().selected == 0, "re-entering switch partner resets to row 0");

  tap(r, 298, 430);
  tap(r, 233, 238);
  check(r.screen().page == Page::PartnerDetail && r.screen().species == 133, "page 2 row 0 opens Eevee");
  select(r, 0);
  check(r.engine().view().partner == 133, "carry Eevee before evolving");
  select(r, 1);
  check(r.screen().page == Page::EvolutionConfirm && r.screen().evolution.to == 134, "Eevee confirms evolution to 134");
  select(r, 0);
  check(r.screen().page == Page::Evolution, "evolution starts");
  edge(r, false, true); edge(r, false, true);
  check(r.screen().page == Page::PartnerDetail && r.screen().species == 134 && r.screen().evolved, "evolution ends on 134 detail");
  tap(r, 112, 52);
  check(r.screen().page == Page::Partners && r.screen().selected == 3, "back after evolution positions on target 134");
}

void firstCapturesPositionAndJump() {
  HostPlatform platform;
  {
    MemoryStore blank; Runtime r(blank, platform); start(r);
    select(r, 1); select(r, 0);
    openMenu(r); select(r, MenuItem::Records); select(r, 0);
    check(r.screen().page == Page::FirstCaptures && r.screen().selected == 0, "new save first captures starts at #001");
  }
  {
    MemoryStore owned; ownedSave649(owned, {1, 25, 133});
    Runtime r(owned, platform); start(r);
    openMenu(r); select(r, MenuItem::Records); select(r, 0);
    check(r.screen().page == Page::FirstCaptures && r.screen().selected == 24, "without recent log, starts at first captured card");
  }
  {
    MemoryStore recent; ownedSave649(recent, {1, 25, 439}, 439);
    Runtime r(recent, platform); start(r);
    openMenu(r); select(r, MenuItem::Records); select(r, 0);
    check(r.screen().page == Page::FirstCaptures && r.screen().selected == 438, "lands on most recent captured species");
    check(rendered_text::contains("#439 魔尼尼", theme::kGold), "recent first capture title renders");
    savePng("first-captures-zh-439");
    edge(r, false, true);
    check(r.screen().selected == 439, "moves to next card");
    tap(r, 112, 52);
    check(r.screen().page == Page::Records, "back from first captures returns to records");
    select(r, 0);
    check(r.screen().page == Page::FirstCaptures && r.screen().selected == 439, "session memory restores previous position");

    const uint16_t expectedStarts[] = {493, 0, 151, 251, 386, 493, 0};
    for (uint16_t expected : expectedStarts) {
      tap(r, 233, 88);
      check(r.screen().page == Page::FirstCaptures && r.screen().selected == expected,
            "tapping #NNN header cycles through all 5 generation starts");
    }
    savePng("first-captures-zh-001");
    frame(r);
    edge(r, true, false);
    check(r.screen().selected == kSpeciesCount, "wraps onto BackToRecords card");
    edge(r, true, true);
    check(r.screen().page == Page::Records, "BackToRecords returns to records");
    select(r, 0);
    check(r.screen().selected == 0, "BackToRecords does not overwrite remembered species index");
    tap(r, 233, 75);
    check(r.screen().page == Page::Records, "y < 80 still triggers top back instead of generation jump");
  }
}

void viewerPrefetchPillAndBackIcon() {
  MemoryStore store; legacyOwned(store, {1});
  HostPlatform platform; Runtime r(store, platform); start(r);
  openMenu(r); select(r, MenuItem::Pokedex);
  check(r.screen().page == Page::Viewer && viewer_screen::species() == 1, "viewer starts at #001");
  check(hasViewerBackIcon(), "top-left back icon is visible with preview button");
  check(rendered_text::contains("妙蛙种子", theme::kText), "Chinese #001 name does not collide with back icon");
  checkCircleBounds("viewer zh 001");
  savePng("viewer-zh-evo-001");

  idle(r, 120);
  check(pokedex::isSpriteCached(pokedex::data::kSprites[1]), "idle frame prefetches next species #002");
  check(pokedex::isSpriteCached(pokedex::data::kSprites[648]), "second idle frame prefetches prev species #649");

  edge(r, true, false);
  check(viewer_screen::species() == 649, "left key wraps to #649");
  check(hasViewerBackIcon(), "top-left back icon is visible without preview button");
  check(rendered_text::contains("盖诺赛克特", theme::kText), "Chinese #649 name is readable");
  checkCircleBounds("viewer zh 649");
  savePng("viewer-zh-noevo-649");
  idle(r, 120);
  check(pokedex::isSpriteCached(pokedex::data::kSprites[647]), "reverse direction prefetches #648 first");

  edge(r, false, true);
  check(viewer_screen::species() == 1, "returns to #001");
  const unsigned fbBefore = platform.feedbacks;
  const uint16_t genTargets[] = {152, 252, 387, 494, 1};
  for (uint16_t expected : genTargets) {
    tap(r, 233, 353);
    check(viewer_screen::species() == expected, "type pill jumps through 5 generation starts");
    check(platform.lastFeedback == 50, "generation pill jump uses Medium (50ms) feedback instead of Strong (80ms) React");
    check(M5.Display.readPixel(416, 154) != 0xF81B, "generation pill jump does not draw React heart");
  }
  check(platform.feedbacks == fbBefore + 5, "each generation jump triggers one haptic");
  tap(r, 233, 220);
  check(viewer_screen::species() == 1 && platform.lastFeedback == 80 &&
        M5.Display.readPixel(416, 154) == 0xF81B,
        "center tap still triggers React heart and Strong (80ms) haptic");
  frame(r);

  auto goSpecies = [&](uint16_t targetId) {
    while (viewer_screen::species() != targetId) edge(r, false, true);
  };
  goSpecies(59);
  check(hasViewerBackIcon() && rendered_text::contains("风速狗", theme::kText), "Chinese Arcanine readable");
  savePng("viewer-zh-long-059");
  goSpecies(609);
  check(hasViewerBackIcon() && rendered_text::contains("水晶灯火灵", theme::kText), "Chinese Chandelure readable");
  savePng("viewer-zh-long-609");

  tap(r, 112, 52);
  check(r.screen().page == Page::Menu && r.screen().selected == unsigned(MenuItem::Pokedex), "top back returns to Pokédex tile");
  select(r, MenuItem::Settings); select(r, 1); select(r, 1);
  check(r.language() == i18n::Language::English, "switched to English");
  tap(r, 112, 52); tap(r, 112, 52);
  select(r, MenuItem::Pokedex);
  check(r.screen().page == Page::Viewer, "English viewer opens");
  tap(r, 233, 353);
  frame(r);
  check(viewer_screen::species() == 1, "wraps from #609 back to #001 in English");
  check(hasViewerBackIcon() && rendered_text::contains("Bulbasaur", theme::kText), "English Bulbasaur readable");
  savePng("viewer-en-evo-001");
  goSpecies(71);
  check(hasViewerBackIcon() && rendered_text::contains("Victreebel", theme::kText), "English Victreebel readable");
  savePng("viewer-en-long-071");
  goSpecies(635);
  check(hasViewerBackIcon() && rendered_text::contains("Hydreigon", theme::kText), "English Hydreigon readable");
  savePng("viewer-en-long-635");
}
}
int main() {
  c152::begin("PROGRESSION TEST"); peakOwnership();
  partnersReturnPosition(); firstCapturesPositionAndJump(); viewerPrefetchPillAndBackIcon();
  std::printf("%u progression Runtime checks, 0 failures\n", checks);
}
