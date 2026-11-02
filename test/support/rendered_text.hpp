#pragma once
#include <M5Unified.h>
#include <algorithm>
#include <vector>

namespace rendered_text {
struct Pixel { int x, y; };
inline bool contains(const char* text, uint16_t color, const lgfx::IFont* font,
                     int top = 0, int bottom = 468) {
  lgfx::LGFX_Sprite reference;
  reference.setColorDepth(16); reference.setFont(font);
  const int width = reference.textWidth(text), height = reference.fontHeight();
  if (width <= 0 || width > 456 || !reference.createSprite(width, height)) return false;
  reference.fillScreen(TFT_BLACK); reference.setTextDatum(top_left);
  reference.setTextColor(TFT_WHITE); reference.drawString(text, 0, 0);
  std::vector<Pixel> ink;
  int left = width, right = 0, first = height, last = 0;
  for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
    if (reference.readPixel(x, y) != TFT_WHITE) continue;
    ink.push_back({x, y}); left = std::min(left, x); right = std::max(right, x);
    first = std::min(first, y); last = std::max(last, y);
  }
  if (ink.size() < 8) return false;
  const Pixel anchors[] = {ink.front(), ink[ink.size() / 3], ink[2 * ink.size() / 3], ink.back()};
  for (int y = top; y + last < bottom; ++y) for (int x = 0; x + right < 468; ++x) {
    bool match = true;
    for (const auto& p : anchors) if (M5.Display.readPixel(x + p.x, y + p.y) != color) { match = false; break; }
    if (!match) continue;
    for (int row = first; match && row <= last; ++row) for (int col = left; col <= right; ++col) {
      if ((reference.readPixel(col, row) == TFT_WHITE) != (M5.Display.readPixel(x + col, y + row) == color)) {
        match = false; break;
      }
    }
    if (match) return true;
  }
  return false;
}
inline bool contains(const char* text, uint16_t color, int top = 0, int bottom = 468) {
  return contains(text, color, &fonts::efontCN_16, top, bottom) ||
         contains(text, color, &fonts::efontCN_24, top, bottom);
}
}
