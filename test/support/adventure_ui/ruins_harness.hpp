#pragma once
#include "harness.hpp"
#include "ruins_catalog.hpp"
#include <cstring>

// Shared fixtures and pixel oracles for the legendary ruins Runtime cases.
namespace {
void ruinsStore(MemoryStore& store, bool unlocked, uint16_t lastTarget = 0,
                uint16_t captured = 0, bool noBalls = false) {
  Save s;
  s.sequence = 4; s.bootGeneration = 1; s.starter = s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  if (unlocked) {
    s.nextEncounterId = 2; s.activityEncounterCount = 1;
    s.ruins.clueMask = kRuinsAllClues; s.ruins.unlocked = true; s.ruins.lastTarget = lastTarget;
  }
  s.totalSteps = s.undatedSteps = 5000;
  s.explorationCredit = 5000;
  if (noBalls) s.rewardBaseSteps = 5000;
  if (captured) {
    setRegistered(s, captured); setOwned(s, captured); s.captureCounts[captured - 1] = 1;
    Capture& card = s.firstCapture[captured - 1];
    card.species = captured; card.route = Route::Forest;
    card.at.bootGeneration = 1; card.at.totalSteps = 500;
  }
  check(validate(s), "ruins UI fixture validates");
  check(store.commit(s) == CommitStatus::Committed, "ruins UI fixture encodes");
}
void ruinsMixedTargetsStore(MemoryStore& store) {
  ruinsStore(store, true, 251, 150);
  Save s; check(store.load(s).status == LoadStatus::Loaded, "mixed target fixture loads");
  s.nextEncounterId = 3; s.activityEncounterCount = 2;
  s.encounter.id = 2; s.encounter.species = s.encounter.opponent = 251;
  s.encounter.partner = 1; s.encounter.route = Route::Forest;
  s.encounter.origin = EncounterOrigin::Ruins; s.phase = Phase::EncounterReady;
  check(validate(s), "mixed target fixture with an open boss validates");
  check(store.commit(s) == CommitStatus::Committed, "mixed target fixture encodes");
}
uint32_t ruinsThrowSeed(uint64_t id, uint16_t species, bool first) {
  for (uint32_t seed = 0; seed < 1000000; ++seed) {
    Encounter probe; probe.seed = seed; probe.id = id; probe.species = species;
    const uint8_t p = captureProbability(catalogEntry(species)->rare, Timing::Perfect);
    if ((throwRoll(probe, 1) < p) == first) return seed;
  }
  return 0;
}
// Pixel oracle for the real efont glyphs. It redraws the same string with the
// same M5GFX font and compares every lit pixel, so a screenshot proves the
// intended copy is actually visible at that position and colour.
bool ruinsText(const char* text, int y, uint16_t color, bool large = true, int width = 290) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  if (!expected.createSprite(468, 40)) return false;
  expected.fillScreen(TFT_BLACK);
  expected.setFont(large ? &fonts::efontCN_24 : &fonts::efontCN_16);
  if (large && width < 290 && expected.textWidth(text) > width) expected.setFont(&fonts::efontCN_16);
  expected.setTextDatum(middle_center);
  expected.setTextColor(color);
  expected.drawString(text, 233, 20);
  unsigned ink = 0;
  // Display and sprite rounding can differ by one row; accept that tolerance
  // while still requiring every lit pixel to match exactly.
  for (int row = 2; row < 36; ++row) for (int px = 40; px < 430; ++px) {
    if (expected.readPixel(px, row)) { ++ink; break; }
  }
  bool same = false;
  for (int shift = -1; shift <= 1 && !same; ++shift) {
    same = true;
    for (int row = 2; row < 36 && same; ++row) for (int px = 40; px < 430; ++px) {
      const uint16_t pixel = expected.readPixel(px, row);
      if (!pixel) continue;
      if (M5.Display.readPixel(px, y - 20 + row + shift) != pixel) { same = false; break; }
    }
  }
  expected.deleteSprite();
  return same && ink > 8;
}
bool ruinsHeading(const char* text) { return ruinsText(text, 52, theme::kText, true, 240); }
bool ruinsLine(const char* text, int y, uint16_t color, bool large = false) {
  return ruinsText(text, y, color, large, 10000);
}
// The shared capture card stacks a mint route row and two 16 px milestone rows
// above the confirm panel (top edge y = 342). Every lit glyph pixel is compared
// against a fresh oracle, so a later row painted over the route cannot pass; the
// joint check also covers frames where both milestone rows are still empty.
void captureCardBadges(Runtime& r, uint8_t mask) {
  char daily[64], lifetime[64];
  const auto& v = r.engine().view();
  const char* route = routeName(v.recent[v.recentSize - 1].route);
  std::snprintf(daily, sizeof(daily), "每日：%s%s%s%s", mask & 1 ? "1千 " : "",
                mask & 2 ? "3千 " : "", mask & 4 ? "5千步" : "",
                mask & 7 ? "" : "尚无里程碑");
  std::snprintf(lifetime, sizeof(lifetime), "累计：%s%s%s%s", mask & 8 ? "1万 " : "",
                mask & 16 ? "5万 " : "", mask & 32 ? "10万步" : "",
                mask & 56 ? "" : "尚无里程碑");
  check(ruinsLine(route, 286, theme::kMint), "capture card keeps the whole route row");
  check(ruinsLine(daily, 304, theme::kGold), "capture card keeps the daily milestone row");
  check(ruinsLine(lifetime, 330, theme::kGold),
        "capture card keeps the whole lifetime milestone row above the confirm panel");
}
// The boss sheet stacks three 16 px spec lines. Every lit glyph pixel is compared
// against a freshly drawn oracle, so the two-item menu panel (top edge y = 208)
// cannot hide a row of the last line without failing these checks.
void ruinsBossSpec(const char* hp, const char* beat, const char* pattern) {
  check(ruinsText(hp, 146, theme::kText, false), "boss detail keeps the whole HP line");
  check(ruinsText(beat, 170, theme::kMuted, false), "boss detail keeps the whole beat line");
  check(ruinsText(pattern, 194, theme::kMuted, false),
        "boss detail keeps the whole pattern line above the menu panel");
}
void ruinsWin(Runtime& r, TestPlatform& p) {
  for (unsigned i = 0; r.engine().view().phase == Phase::Battle && i < 15; ++i) {
    auto b = r.engine().battleView();
    while (int32_t(b.targetMs - host_time_ms) > 20) frame(r);
    p.next.left = b.expected == Action::Dodge; p.next.right = b.expected == Action::Attack;
    p.next.held = true; frame(r, b.targetMs - host_time_ms); frame(r);
    if (r.engine().view().phase == Phase::Battle) idle(r, 200);
  }
  check(r.engine().view().phase != Phase::Battle, "ruins battle settles");
  idle(r, 600);  // let the final action animation finish before the result page
}
void openRuins(Runtime& r, TestPlatform& p) {
  openMenu(r, p); select(r, p, 7);
  check(r.screen().page == Page::Ruins, "main menu opens the legendary ruins entry");
}
void backToMenu(Runtime& r, TestPlatform& p) {
  idle(r, 60); tap(r, p, 233, 30);
  check(r.screen().page == Page::Menu, "ruins back entry returns to the menu");
}
void throwMenuItem(Runtime& r, TestPlatform& p, unsigned item) {
  for (unsigned guard = 0; r.screen().selected != item && guard < 5; ++guard) edge(r, p, true, false);
  check(r.screen().page == Page::Throw && r.screen().selected == item,
        "throw menu item reachable with physical keys");
  edge(r, p, true, true);
}
}
