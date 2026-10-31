#pragma once
#include "board.hpp"
#include "screen_theme.hpp"
#include <cstdio>

namespace animation_feedback_test {
inline bool label(const char* text, uint16_t color, int centerY = 399) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  if (!expected.createSprite(280, 32)) return false;
  expected.fillScreen(TFT_BLACK);
  expected.setFont(&fonts::efontCN_24);
  expected.setTextDatum(middle_center);
  expected.setTextColor(color);
  expected.drawString(text, 140, 16);
  unsigned ink = 0;
  for (int y = 0; y < 32; ++y) for (int x = 0; x < 280; ++x) {
    if (expected.readPixel(x, y) != color) continue;
    ++ink;
    if (M5.Display.readPixel(93 + x, centerY - 16 + y) != color) {
      std::fprintf(stderr, "label %s mismatch at %d,%d: expected %04x actual %04x\n", text, 93 + x, centerY - 16 + y, color, unsigned(M5.Display.readPixel(93 + x, centerY - 16 + y)));
      return false;
    }
  }
  return ink > 20;
}
}
