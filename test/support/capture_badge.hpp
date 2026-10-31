#pragma once
#include "board.hpp"
#include "icons_generated.hpp"
#include "screen_theme.hpp"
#include "sprite.hpp"
#include <cstdio>

namespace capture_badge_test {
inline bool iconAt(int x, int y) {
  const auto& icon = pokedex::data::kPokeBallIcon;
  for (unsigned row = 0; row < icon.height; ++row)
    for (unsigned col = 0; col < icon.width; ++col) {
      const auto pixel = icon.pixels[row * icon.width + col];
      if (pixel != pokedex::kTransparentKey &&
          M5.Display.readPixel(x - icon.width / 2 + col, y - icon.height / 2 + row) != pixel)
        return false;
    }
  return true;
}
inline bool countAt(unsigned count, int x, int y) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  if (!expected.createSprite(48, 24)) return false;
  expected.fillScreen(TFT_BLACK);
  expected.setTextDatum(middle_left);
  expected.setFont(&fonts::Font2);
  expected.setTextColor(pokedex::ui::theme::kText);
  char text[8]; std::snprintf(text, sizeof(text), "%u", count);
  expected.drawString(text, 0, 12);
  bool matches = true;
  for (int row = 0; row < 24; ++row) for (int col = 0; col < 48; ++col) {
    const bool ink = expected.readPixel(col, row) == pokedex::ui::theme::kText;
    const bool actual = M5.Display.readPixel(x + 23 + col, y - 12 + row) == pokedex::ui::theme::kText;
    if (ink != actual) matches = false;
  }
  return matches;
}
inline bool badge(unsigned count, int x, int y) {
  return iconAt(x, y) && countAt(count, x, y);
}
}
