#include "screens.hpp"
#include "board.hpp"
#include "scene_backgrounds_generated.hpp"
#include "battle_presentation.hpp"
#include <algorithm>
#include <cmath>

namespace pokedex { namespace ui {
using namespace adventure;
static_assert(data::kSceneCount == kRegionCount, "one scene per region");
namespace {
// Legacy type routes survive only in capture history and never select a scene.
unsigned sceneIndex(Route route) { return isRegion(route) ? regionIndex(route) : 0; }
}
void Screens::line(const char* text, int y, uint16_t color, bool large) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(large ? &fonts::efontCN_24 : &fonts::efontCN_16);
  if (large) {
    const int dy = std::abs(y - 233) + 12;
    const int width = dy < 220 ? int(2 * std::sqrt(float(220 * 220 - dy * dy))) : 0;
    if (M5.Display.textWidth(text) > width) M5.Display.setFont(&fonts::efontCN_16);
  }
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
  const unsigned index = sceneIndex(route);
  const auto& source = dim ? data::kRegionScenesDim[index] : data::kRegionScenes[index];
  const auto& layers = data::kRegionSceneLayers[index];
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
      if (!sceneEdgeVisible(x, y)) pixel = theme::kBackground;
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
  M5.Display.pushImage(cx - 114, cy - 45, data::kSceneWidth, data::kSceneThumbHeight,
                       data::kRegionScenes[sceneIndex(route)].pixels + data::kSceneThumbTop * data::kSceneWidth);
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
void Screens::hp(const char* name, uint8_t value, uint8_t total, bool enemy, uint16_t color) {
  const auto& plate = enemy ? kEnemyPlate : kPlayerPlate;
  M5.Display.fillRoundRect(plate.x, plate.y, plate.width, plate.height, plate.height / 2, theme::kBackground);
  const int left = plate.x + kPlateInset, right = plate.x + plate.width - kPlateInset;
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextColor(theme::kText);
  M5.Display.setTextDatum(enemy ? middle_right : middle_left);
  M5.Display.setClipRect(left, plate.y, right - left, plate.height);
  M5.Display.drawString(name, enemy ? right : left, plate.y + 12);
  M5.Display.clearClipRect();
  const int start = enemy ? right - hpTrackWidth(total) : left;
  for (unsigned index = 0; index < total; ++index)
    M5.Display.fillRoundRect(start + hpSegmentStart(index, total), kHpSegmentY, hpSegmentWidth(total), 6, 2,
                             index < value ? color : theme::kDisabled);
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
