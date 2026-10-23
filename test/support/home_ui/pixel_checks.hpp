#pragma once
#include "../progression_ui.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
using namespace progression_ui;
namespace {
void ready(MemoryStore& store, uint16_t starter = 1) {
  Engine engine(store);
  check(engine.begin() == CommandResult::Applied, "home fixture boots through Engine");
  check(engine.chooseStarter(starter) == CommandResult::Applied, "home fixture chooses a real starter");
}
void settle(Runtime& runtime) { buttons(false, false); frame(runtime); frame(runtime); }
void home(Runtime& runtime) {
  check(runtime.screen().page == Page::Home, "exploration remains the visible page");
  check(menuCount(runtime.screen(), runtime.engine()) == 0, "exploration has no invisible menu");
}
void roundPixels() {
  bool safe = true;
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x)
    if ((x - 233) * (x - 233) + (y - 233) * (y - 233) > 228 * 228 && M5.Display.readPixel(x, y)) safe = false;
  check(safe, "complete framebuffer fits radius 228 on the circular screen");
}
bool textLine(const char* text, int y, uint16_t color, int width = 290) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  check(expected.createSprite(468, 40), "allocate text oracle");
  expected.fillScreen(TFT_BLACK); expected.setFont(&fonts::efontCN_24);
  if (expected.textWidth(text) > width) expected.setFont(&fonts::efontCN_16);
  expected.setTextDatum(middle_center); expected.setTextColor(color, TFT_BLACK);
  expected.drawString(text, 233, 20);
  bool same = true; unsigned ink = 0;
  for (int row = 2; row < 36; ++row) for (int x = 80; x < 386; ++x) {
    const uint16_t pixel = expected.readPixel(x, row); ink += pixel != 0;
    if (M5.Display.readPixel(x, y - 20 + row) != pixel) same = false;
  }
  expected.deleteSprite(); return ink > 8 && same;
}
unsigned frameAt(const Sprite& sprite, uint32_t elapsed) {
  uint32_t position = elapsed % sprite.duration_ms;
  for (unsigned i = 0; i < sprite.frame_count; ++i) {
    if (position < sprite.frames[i].duration_ms) return i;
    position -= sprite.frames[i].duration_ms;
  }
  check(false, "source durations cover the complete cycle"); return 0;
}
void spritePixels(uint16_t id, uint32_t origin, bool forestTransparency = false) {
  const auto& sprite = data::kSprites[id - 1];
  const unsigned scale = displayScale(sprite), width = sprite.width * scale, height = sprite.height * scale;
  std::vector<uint16_t> source(sprite.width * sprite.height);
  check(decodeFrame(sprite, frameAt(sprite, host_time_ms - origin), 1, source.data(), source.size()),
        "pixel oracle decodes original source frame at original duration");
  const int left = 233 - int(width) / 2, top = 233 - int(height) / 2;
  bool opaque = true, transparent = true; unsigned ink = 0, holes = 0;
  for (unsigned y = 0; y < sprite.height; ++y) for (unsigned x = 0; x < sprite.width; ++x) {
    const uint16_t pixel = source[y * sprite.width + x];
    for (unsigned dy = 0; dy < scale; ++dy) for (unsigned dx = 0; dx < scale; ++dx) {
      const uint16_t actual = M5.Display.readPixel(left + x * scale + dx, top + y * scale + dy);
      if (pixel != kTransparentKey) { ++ink; opaque &= actual == pixel; }
      else { ++holes; transparent &= actual != kTransparentKey;
        if (forestTransparency) transparent &= actual == 0x0924;
      }
    }
  }
  check(ink && opaque, "all opaque source pixels retain exact RGB565 and integer scaling");
  check(holes && transparent, "transparent pixels expose the scene without magenta or black erasure");
}
}
