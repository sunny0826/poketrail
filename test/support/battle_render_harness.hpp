#pragma once
#include "progression_ui.hpp"
#include "battle_presentation.hpp"
#include "round_layout_checks.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
#include <cstdio>
#include <string>
#include <vector>

namespace battle_render_test {
using namespace progression_ui;
inline void fixture(MemoryStore& store, uint16_t partner = 4, uint16_t enemy = 25, Route route = Route::Coast) {
  Save save; save.bootGeneration = 1; save.starter = 1; save.partner = partner;
  save.rosterRevision = 1; setOwned(save, 1); setRegistered(save, 1);
  setOwned(save, partner); setRegistered(save, partner);
  save.encounterTarget = 300;
  save.totalSteps = save.undatedSteps = 5000; save.explorationSpent = 5000; save.nextEncounterId = 2;
  save.phase = Phase::EncounterReady; save.route = save.encounter.route = route;
  save.encounter.id = 1; save.encounter.species = enemy; save.encounter.partner = partner;
  check(validate(save) && store.commit(save) == CommitStatus::Committed, "battle fixture validates through production codec");
}
inline uint32_t enter(Runtime& r, bool practice = false) {
  openMenu(r);
  if (!practice) select(r, 0);
  else for (unsigned i = 0; i < 5; ++i) edge(r, false, true);
  buttons(true, true); frame(r);
  const uint32_t origin = host_time_ms;
  check(r.screen().page == (practice ? Page::Practice : Page::Battle), "real chord enters battle surface");
  buttons(false, false); frame(r); frame(r);
  return origin;
}
inline void at(Runtime& r, uint32_t time) { frame(r, time - host_time_ms); }
inline void action(Runtime& r, Action action, uint32_t time) {
  buttons(action == Action::Dodge, action == Action::Attack); at(r, time);
  buttons(false, false); frame(r, 0);
}
inline unsigned frameIndex(const Sprite& sprite, uint32_t elapsed) {
  uint32_t t = elapsed % sprite.duration_ms;
  for (unsigned i = 0; i < sprite.frame_count; ++i) {
    if (t < sprite.frames[i].duration_ms) return i;
    t -= sprite.frames[i].duration_ms;
  }
  check(false, "source frame timing is complete"); return 0;
}
inline void actor(uint16_t id, uint32_t origin, int cx, int cy, unsigned scale, bool upperOnly = false, uint16_t silhouette = 0) {
  const auto& sprite = data::kSprites[id - 1];
  const unsigned width = sprite.width * scale, height = sprite.height * scale;
  std::vector<uint16_t> pixels(width * height);
  check(decodeFrame(sprite, frameIndex(sprite, host_time_ms - origin), scale, pixels.data(), pixels.size()), "original animation frame decodes");
  unsigned ink = 0, mismatch = 0;
  for (unsigned y = 0; y < (upperOnly ? height / 4 : height); ++y) for (unsigned x = 0; x < width; ++x) {
    const uint16_t pixel = pixels[y * width + x];
    if (pixel == kTransparentKey) continue;
    ++ink;
    if (M5.Display.readPixel(cx - int(width) / 2 + x, cy - int(height) / 2 + y) != (silhouette ? silhouette : pixel)) ++mismatch;
  }
  if (mismatch) std::fprintf(stderr, "actor #%u at %u origin %u center %d,%d mismatches=%u/%u\n", id, host_time_ms, origin, cx, cy, mismatch, ink);
  check(ink > 20 && mismatch == 0, "visible actor preserves original frame duration, integer pixels and placement");
}
inline void hp(unsigned player, unsigned enemy) {
  const int y = kHpSegmentY + 3;
  for (unsigned i = 0; i < kPlayerHp; ++i)
    check(M5.Display.readPixel(hpSegmentX(false, i, kPlayerHp), y) == (i < player ? theme::kMint : theme::kDisabled), "player HP immediately matches battle result");
  for (unsigned i = 0; i < kEnemyHp; ++i)
    check(M5.Display.readPixel(hpSegmentX(true, i, kEnemyHp), y) == (i < enemy ? theme::kGold : theme::kDisabled), "enemy HP immediately matches battle result");
  for (const auto& plate : {kPlayerPlate, kEnemyPlate})
    check(round_layout_test::insideRowRadius(plate) &&
          M5.Display.readPixel(plate.x + plate.width / 2, plate.y + 2) == theme::kBackground,
          "battle name plates sit on solid backing inside the round screen");
}
// Plays cues until the next one requires `want`, using perfect dodges and normal attacks.
inline void until(Runtime& r, Action want) {
  while (r.engine().battleView().expected != want) {
    const auto b = r.engine().battleView();
    action(r, b.expected, b.targetMs - (b.expected == Action::Attack ? 100 : 0));
    at(r, host_time_ms + 450);
  }
}
inline void snapshot(const char* name) {
  const std::string path = verificationOutput((std::string(name) + ".ppm").c_str());
  FILE* file = std::fopen(path.c_str(), "wb"); check(file, "open battle screenshot");
  std::fprintf(file, "P6\n468 468\n255\n"); bool safe = true;
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x) {
    const uint16_t pixel = M5.Display.readPixel(x, y);
    if (pixel && (x - 233) * (x - 233) + (y - 233) * (y - 233) > 228 * 228) safe = false;
    const uint8_t rgb[] = {uint8_t((pixel >> 11) * 255 / 31), uint8_t(((pixel >> 5) & 63) * 255 / 63), uint8_t((pixel & 31) * 255 / 31)};
    std::fwrite(rgb, 1, 3, file);
  }
  std::fclose(file); check(safe, "battle frame fits circular screen");
}
}
