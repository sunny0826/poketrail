#pragma once
#include "progression_ui.hpp"
#include "battle_presentation.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
using namespace progression_ui;

// Shared battle fixture, input drivers, actor oracle and HP probes.
namespace {
void fixture(MemoryStore& store, uint16_t partner = 4, uint16_t enemy = 25, Route route = Route::Forest) {
  Save save; save.bootGeneration = 1; save.starter = 1; save.partner = partner;
  save.rosterRevision = 1; setOwned(save, 1); setRegistered(save, 1);
  setOwned(save, partner); setRegistered(save, partner);
  save.totalSteps = save.undatedSteps = 5000; save.nextEncounterId = 2;
  save.phase = Phase::EncounterReady; save.route = save.encounter.route = route;
  save.encounter.id = 1; save.encounter.species = enemy; save.encounter.partner = partner;
  check(validate(save) && store.commit(save) == CommitStatus::Committed, "battle fixture validates through production codec");
}
uint32_t enter(Runtime& r, bool practice = false) {
  openMenu(r);
  if (!practice) select(r, 0);
  else for (unsigned i = 0; i < 5; ++i) edge(r, false, true);
  buttons(true, true); frame(r);
  const uint32_t origin = host_time_ms;
  check(r.screen().page == (practice ? Page::Practice : Page::Battle), "real chord enters battle surface");
  buttons(false, false); frame(r); frame(r);
  return origin;
}
void at(Runtime& r, uint32_t time) { frame(r, time - host_time_ms); }
void action(Runtime& r, Action action, uint32_t time) {
  buttons(action == Action::Dodge, action == Action::Attack); at(r, time);
  buttons(false, false); frame(r, 0);
}
unsigned frameIndex(const Sprite& sprite, uint32_t elapsed) {
  uint32_t t = elapsed % sprite.duration_ms;
  for (unsigned i = 0; i < sprite.frame_count; ++i) {
    if (t < sprite.frames[i].duration_ms) return i;
    t -= sprite.frames[i].duration_ms;
  }
  check(false, "source frame timing is complete"); return 0;
}
void actor(uint16_t id, uint32_t origin, int cx, int cy, unsigned scale, bool upperOnly = false, uint16_t silhouette = 0) {
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
void hp(unsigned player, unsigned enemy) {
  for (unsigned i = 0; i < 3; ++i) {
    check(M5.Display.readPixel(92 + i * 18, 110) == (i < player ? theme::kMint : theme::kDisabled), "player HP immediately matches battle result");
    check(M5.Display.readPixel(283 + i * 18, 110) == (i < enemy ? theme::kGold : theme::kDisabled), "enemy HP immediately matches battle result");
  }
}
void snapshot(const char* name) {  const std::string path = verificationOutput((std::string(name) + ".ppm").c_str());
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
void drawPractice(Screens& screens, Engine& engine, const BattleView& view, uint32_t now) {
  ScreenState state; state.page = Page::Practice; state.practiceSpecies = 25;
  check(screens.draw(state, engine, view, now), "production Practice screen renders a supplied battle view");
}
std::vector<uint16_t> region(int top, int bottom) {
  std::vector<uint16_t> pixels;
  for (int y = top; y < bottom; ++y) for (int x = 0; x < 468; ++x) pixels.push_back(M5.Display.readPixel(x, y));
  return pixels;
}
// Custom maximum HP must reach the bars: 49 px of track, filled in proportion.
void hpBar(int x, uint16_t color, unsigned value, unsigned max, const char* message) {
  const unsigned fill = max ? value * 49 / max : 0;
  if (fill) check(M5.Display.readPixel(x, 110) == color, message);
  else check(M5.Display.readPixel(x, 110) == theme::kDisabled, message);
  if (fill) check(M5.Display.readPixel(x + int(fill) - 1, 110) == color, message);
  if (fill < 49) check(M5.Display.readPixel(x + int(fill), 110) == theme::kDisabled, message);
}
// The bar capacity is the real maximum, not always three slots. A maximum of
// one or two must paint exactly that many segments and leave the rest of the
// row untouched, so full and damaged states stay distinguishable.
void hpSegments(int x, uint16_t color, unsigned value, unsigned max, const char* message) {
  for (unsigned slot = 0; slot < max; ++slot)
    check(M5.Display.readPixel(x + int(slot * 18), 110) == (slot < value ? color : theme::kDisabled), message);
  const uint16_t beyond = M5.Display.readPixel(x + int(max * 18), 110);
  check(beyond != color && beyond != theme::kDisabled, message);
}
void dumpHpRow(const char* name) {
  const std::string path = verificationOutput((std::string(name) + ".bin").c_str());
  FILE* file = std::fopen(path.c_str(), "wb"); check(file, "open HP bar row dump");
  for (int y = 104; y < 117; ++y) for (int x = 80; x < 360; ++x) {
    const uint16_t pixel = M5.Display.readPixel(x, y);
    std::fwrite(&pixel, sizeof(pixel), 1, file);
  }
  std::fclose(file);
}
