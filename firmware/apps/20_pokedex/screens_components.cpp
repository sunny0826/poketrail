#include "screens.hpp"
#include "board.hpp"
#include "scene_backgrounds_generated.hpp"
#include <algorithm>
#include <cmath>

namespace pokedex { namespace ui {
using namespace adventure;

void Screens::line(const char* text, int y, uint16_t color, bool large) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(large ? &fonts::efontCN_24 : &fonts::efontCN_16);
  M5.Display.setTextColor(color);
  M5.Display.drawString(text, 233, y);
}
void Screens::fitLine(const char* text, int y, int width, uint16_t color, bool large) {
  M5.Display.setFont(large ? &fonts::efontCN_24 : &fonts::efontCN_16);
  if (large && M5.Display.textWidth(text) > width) M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(color);
  M5.Display.drawString(text, 233, y);
}
void Screens::title(const char* heading, const char* subtitle, uint16_t accent) {
  fitLine(heading, 52, 240, theme::kText, true);
  if (subtitle && *subtitle) fitLine(subtitle, 80, 300, accent, false);
}
void Screens::scene(Route route, bool dim, int dx, uint64_t visualSteps) {
  const unsigned index = uint8_t(route) < 3 ? uint8_t(route) : 0;
  const auto& source = dim ? data::kRouteScenesDim[index] : data::kRouteScenes[index];
  const auto& layers = data::kRouteSceneLayers[index];
  const unsigned groundTop = layers.groundTop;
  const unsigned groundHeight = data::kSceneHeight - groundTop;
  const bool depth = !dim && visualSteps;
  const unsigned middleOffset = depth ? visualSteps % groundHeight : 0;
  const unsigned nearOffset = depth ? (visualSteps % groundHeight * 2) % groundHeight : 0;
  for (unsigned y = 0; y < data::kSceneHeight; ++y) {
    const uint16_t* row = depth ? layers.far.pixels + y * data::kSceneWidth
                                : source.pixels + y * data::kSceneWidth;
    const bool ground = depth && y >= groundTop;
    const unsigned middleY = ground ? groundTop + (y - groundTop + groundHeight - middleOffset) % groundHeight : groundTop;
    const unsigned nearY = ground ? groundTop + (y - groundTop + groundHeight - nearOffset) % groundHeight : groundTop;
    const uint16_t* middle = layers.middle.pixels + middleY * data::kSceneWidth;
    const uint16_t* near = layers.near.pixels + nearY * data::kSceneWidth;
    for (unsigned x = 0; x < data::kSceneWidth; ++x) {
      uint16_t pixel = row[x];
      if (ground && middle[x] != data::kSceneTransparentKey) pixel = middle[x];
      if (ground && near[x] != data::kSceneTransparentKey) pixel = near[x];
      sceneRow_[x * 2] = sceneRow_[x * 2 + 1] = pixel;
    }
    for (unsigned dy = 0; dy < 2; ++dy) {
      const int screenY = 92 + y * 2 + dy;
      const int vertical = screenY - 233;
      const int span = int(std::sqrt(float(228 * 228 - vertical * vertical)));
      const int left = 233 - span;
      M5.Display.setClipRect(left, screenY, span * 2 + 1, 1);
      M5.Display.pushImage(5 + dx, screenY, 456, 1, sceneRow_);
    }
  }
  M5.Display.clearClipRect();
}
void Screens::sceneThumbnail(Route route, int cx, int cy) {
  const unsigned index = uint8_t(route) < 3 ? uint8_t(route) : 0;
  M5.Display.pushImage(cx - 114, cy - 45, data::kSceneWidth, data::kSceneThumbHeight,
                       data::kRouteScenes[index].pixels + data::kSceneThumbTop * data::kSceneWidth);
}
void Screens::glyph(theme::Icon icon, int x, int y, uint16_t color) {
  switch (icon) {
    case theme::Icon::Route:
      M5.Display.drawLine(x - 9, y + 7, x - 2, y - 5, color);
      M5.Display.drawLine(x - 2, y - 5, x + 3, y + 2, color);
      M5.Display.drawLine(x + 3, y + 2, x + 7, y - 4, color);
      M5.Display.drawLine(x + 7, y - 4, x + 10, y + 7, color);
      M5.Display.drawLine(x - 9, y + 7, x + 10, y + 7, color);
      break;
    case theme::Icon::Partner:
      M5.Display.drawCircle(x, y + 1, 7, color);
      M5.Display.fillRect(x - 8, y - 9, 5, 5, color);
      M5.Display.fillRect(x + 3, y - 9, 5, 5, color);
      M5.Display.drawPixel(x - 3, y, color); M5.Display.drawPixel(x + 3, y, color);
      break;
    case theme::Icon::Book:
      M5.Display.drawRect(x - 8, y - 8, 16, 17, color);
      M5.Display.drawLine(x - 3, y - 8, x - 3, y + 8, color);
      break;
    case theme::Icon::Records:
      for (int dy : {-6, 0, 6}) {
        M5.Display.fillRect(x - 7, y + dy, 3, 2, color);
        M5.Display.drawLine(x, y + dy + 1, x + 8, y + dy + 1, color);
      }
      break;
    case theme::Icon::Clock:
      M5.Display.drawCircle(x, y, 8, color);
      M5.Display.drawLine(x, y, x, y - 5, color);
      M5.Display.drawLine(x, y, x + 4, y + 3, color);
      break;
    case theme::Icon::Practice:
      M5.Display.drawCircle(x, y, 8, color);
      M5.Display.drawCircle(x, y, 3, color);
      break;
    case theme::Icon::Sleep:
      M5.Display.drawLine(x - 6, y - 7, x + 6, y - 7, color);
      M5.Display.drawLine(x + 6, y - 7, x - 6, y + 7, color);
      M5.Display.drawLine(x - 6, y + 7, x + 6, y + 7, color);
      break;
    case theme::Icon::Home:
      M5.Display.drawLine(x - 9, y, x, y - 8, color);
      M5.Display.drawLine(x, y - 8, x + 9, y, color);
      M5.Display.drawRect(x - 6, y, 13, 9, color);
      break;
    case theme::Icon::Back:
    case theme::Icon::Next: {
      const int direction = icon == theme::Icon::Back ? -1 : 1;
      M5.Display.drawLine(x - 7, y, x + 7, y, color);
      M5.Display.drawLine(x + direction * 7, y, x + direction, y - 6, color);
      M5.Display.drawLine(x + direction * 7, y, x + direction, y + 6, color);
      break;
    }
    default:
      M5.Display.drawCircle(x, y, 7, color);
      M5.Display.drawLine(x - 7, y, x + 7, y, color);
      break;
  }
}
void Screens::alert(const char* text, int y, uint16_t color) {
  M5.Display.setFont(&fonts::efontCN_16);
  const int width = std::min(330, int(M5.Display.textWidth(text)) + 32);
  M5.Display.fillRoundRect(233 - width / 2, y - 15, width, 30, 8, theme::kAlertBackground);
  fitLine(text, y, width - 18, color, false);
}
void Screens::progress(uint64_t value, uint32_t total, int y, uint16_t color) {
  const unsigned filled = total && value < total ? (value * 12 + total / 2) / total : (total ? 12 : 0);
  for (unsigned index = 0; index < 12; ++index) {
    const int x = 133 + index * 18;
    M5.Display.fillRect(x, y, 13, 5, index < filled ? color : theme::kDisabled);
  }
}
void Screens::hp(const char* name, uint8_t value, uint8_t max, int x, uint16_t color) {
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextColor(theme::kText);
  M5.Display.drawString(name, x, 92);
  // The original three-segment bar stays pixel-identical for the default 3/3.
  // Smaller maxima keep one segment per point of health instead of always
  // drawing three, so a full 1/1 or 2/2 bar cannot look already damaged.
  if (max <= 3) {
    const unsigned slots = max ? max : 3;
    for (unsigned index = 0; index < slots; ++index)
      M5.Display.fillRect(x + index * 18, 108, 13, 7, index < value ? color : theme::kDisabled);
    return;
  }
  // Larger maxima need one compact 49 px track: a proportional share is the only
  // way 5 -> 4 -> 3 stays readable inside the 466 px circle.
  constexpr int kWidth = 49;
  M5.Display.fillRect(x, 108, kWidth, 7, theme::kDisabled);
  unsigned filled = max ? unsigned(value) * kWidth / max : 0;
  if (filled > unsigned(kWidth)) filled = kWidth;
  if (filled) M5.Display.fillRect(x, 108, int(filled), 7, color);
}
void Screens::actorShadow(int cx, int cy) {
  M5.Display.fillEllipse(cx, cy + 67, 40, 6, theme::rgb(0x6C, 0x68, 0x48));
}
void Screens::warning(int cx, int cy, uint16_t color) {
  M5.Display.drawTriangle(cx, cy - 28, cx - 31, cy + 27, cx + 31, cy + 27, color);
  M5.Display.setTextDatum(middle_center); M5.Display.setFont(&fonts::efontCN_24);
  M5.Display.setTextColor(color); M5.Display.drawString("!", cx, cy + 11);
}
} }
