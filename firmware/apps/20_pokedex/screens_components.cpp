#include "screens.hpp"
#include "board.hpp"
#include "icons_generated.hpp"
#include "scene_backgrounds_generated.hpp"
#include "battle_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
static_assert(data::kSceneCount == kRegionCount, "one scene per region");
static_assert(data::kSceneWidth * data::kSceneScale == 456, "scene display width matches row buffer");
static_assert(data::kSceneHeight * data::kSceneScale == 270, "scene display height matches lookup table");
namespace {
constexpr unsigned kSceneDisplayWidth = data::kSceneWidth * data::kSceneScale;
constexpr unsigned kSceneDisplayHeight = data::kSceneHeight * data::kSceneScale;
const uint8_t* sceneHalfSpan() {
  static uint8_t table[kSceneDisplayHeight];
  static bool ready = false;
  if (!ready) {
    for (unsigned row = 0; row < kSceneDisplayHeight; ++row) {
      const int vertical = 92 + int(row) - c152::kScreenCenter;
      table[row] = uint8_t(std::sqrt(float(c152::kVisibleRadius * c152::kVisibleRadius - vertical * vertical)));
    }
    ready = true;
  }
  return table;
}
unsigned sceneIndex(Route route) { return isRegion(route) ? regionIndex(route) : 0; }
uint16_t accentFor(Page page) {
  switch (page) {
    case Page::DateWelcome: case Page::Starter: case Page::Encounter: case Page::Pause:
    case Page::Throw: case Page::Result: case Page::Records: case Page::FirstCaptures:
    case Page::Recent: case Page::Days: case Page::Achievements: case Page::EvolutionConfirm:
    case Page::EvolutionPreviewTargets:
      return theme::kGold;
    case Page::Error: return theme::kRed;
    case Page::TestEntry: case Page::TestHub: case Page::TestSteps: case Page::TestScenarios:
    case Page::TestFaults: case Page::TestDiagnostics: case Page::TestExit: return theme::kPurple;
    case Page::Clock: return theme::kBlue;
    default: return theme::kMint;
  }
}
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
  const uint8_t* halfSpans = sceneHalfSpan();
  const unsigned index = sceneIndex(route);
  const auto& source = dim ? data::kRegionScenesDim[index] : data::kRegionScenes[index];
  const auto& layers = data::kRegionSceneLayers[index];
  const unsigned groundTop = layers.groundTop;
  const unsigned groundHeight = data::kSceneHeight - groundTop;
  const bool depth = !dim && visualSteps;
  if (!depth && sceneCache_) {
    if (!sceneCacheValid_ || sceneCacheRoute_ != route || sceneCacheDim_ != dim) {
      for (unsigned y = 0; y < data::kSceneHeight; ++y) {
        const uint16_t* row = source.pixels + y * data::kSceneWidth;
        uint16_t* dst = sceneCache_ + size_t(y * data::kSceneScale) * kSceneDisplayWidth;
        for (unsigned x = 0; x < data::kSceneWidth; ++x) {
          const uint16_t pixel = sceneEdgeVisible(x, y) ? row[x] : theme::kBackground;
          dst[x * 2] = dst[x * 2 + 1] = pixel;
        }
        std::memcpy(dst + kSceneDisplayWidth, dst, kSceneDisplayWidth * sizeof(uint16_t));
      }
      sceneCacheRoute_ = route;
      sceneCacheDim_ = dim;
      sceneCacheValid_ = true;
    }
    unsigned row = 0;
    while (row < kSceneDisplayHeight) {
      const int span = halfSpans[row];
      unsigned end = row + 1;
      while (end < kSceneDisplayHeight && halfSpans[end] == span) ++end;
      const int left = c152::kScreenCenter - span;
      M5.Display.setClipRect(left, 92 + int(row), span * 2 + 1, int(end - row));
      M5.Display.pushImage(5 + dx, 92 + int(row), int(kSceneDisplayWidth), int(end - row),
                           sceneCache_ + size_t(row) * kSceneDisplayWidth);
      row = end;
    }
    M5.Display.clearClipRect();
    return;
  }
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
    for (unsigned dy = 0; dy < data::kSceneScale; ++dy) {
      const unsigned displayRow = y * data::kSceneScale + dy;
      const int screenY = 92 + int(displayRow);
      const int span = halfSpans[displayRow];
      const int left = c152::kScreenCenter - span;
      M5.Display.setClipRect(left, screenY, span * 2 + 1, 1);
      M5.Display.pushImage(5 + dx, screenY, int(kSceneDisplayWidth), 1, sceneRow_);
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
void Screens::thumbnail(uint16_t id, int x, int y, unsigned scale) {
  if (!id || id > kSpeciesCount || !scale) return;
  const auto& icon = data::kPokemonIcons[id - 1];
  const int left = x - icon.width * int(scale) / 2;
  const int top = y - icon.height * int(scale) / 2;
  if (scale == 1) {
    M5.Display.pushImage(left, top, icon.width, icon.height, icon.pixels, kTransparentKey);
    return;
  }
  const size_t scaledWidth = size_t(icon.width) * scale;
  if (scaledWidth <= sizeof(sceneRow_) / sizeof(sceneRow_[0])) {
    for (unsigned row = 0; row < icon.height; ++row) {
      const uint16_t* src = icon.pixels + row * icon.width;
      uint16_t* dst = sceneRow_;
      for (unsigned col = 0; col < icon.width; ++col, dst += scale)
        std::fill_n(dst, scale, src[col]);
      for (unsigned dy = 0; dy < scale; ++dy)
        M5.Display.pushImage(left, top + int(row * scale + dy), int(scaledWidth), 1,
                             sceneRow_, kTransparentKey);
    }
    return;
  }
  for (unsigned row = 0; row < icon.height; ++row)
    for (unsigned col = 0; col < icon.width; ++col) {
      const uint16_t color = icon.pixels[row * icon.width + col];
      if (color != kTransparentKey)
        M5.Display.fillRect(left + col * scale, top + row * scale, scale, scale, color);
    }
}
void Screens::captureBadge(const Save& save, int x, int y) {
  const auto& icon = data::kPokeBallIcon;
  M5.Display.pushImage(x - icon.width / 2, y - icon.height / 2, icon.width, icon.height,
                       icon.pixels, kTransparentKey);
  char text[8];
  snprintf(text, sizeof(text), "%u", capturedSpeciesCount(save));
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(theme::kText);
  M5.Display.drawString(text, x + icon.width / 2 + 8, y);
}
void Screens::encounterBadge(const Save& save, int x, int y) {
  const unsigned count = pendingEncounterCount(save);
  if (!count) return;
  char text[32];
  snprintf(text, sizeof(text), tr(Text::EncounterCount), count);
  M5.Display.setTextDatum(middle_right);
  M5.Display.setFont(&fonts::efontCN_16);
  const int width = M5.Display.textWidth(text) + 16;
  M5.Display.fillRoundRect(x + 8 - width, y - 12, width, 24, 7, theme::kPanel);
  M5.Display.setTextColor(theme::kGold);
  M5.Display.drawString(text, x, y);
}
void Screens::panelFrame(const menu_layout::Rect& b, int radius, bool active, bool pressed, uint16_t accent) {
  const int in = pressed ? 2 : 0;
  const int r = std::max(2, radius - in);
  const int x = b.x + in, y = b.y + in, w = b.width - in * 2, h = b.height - in * 2;
  const uint16_t bg = pressed ? kPanelPressed : active ? theme::kPanelActive : theme::kPanel;
  M5.Display.fillRoundRect(x, y, w, h, r, bg);
  if (!active && !pressed) return;
  M5.Display.drawRoundRect(x, y, w, h, r, pressed && active ? theme::kText : accent);
  M5.Display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, std::max(1, r - 1), accent);
}
void Screens::item(const ScreenState& s, unsigned index, const char* label, int top,
                   uint16_t species, bool carried, const char* note, bool disabled) {
  const unsigned first = menu_layout::first(s.selected);
  if (index < first || index >= first + menu_layout::kPageSize) return;
  const auto bounds = menu_layout::rowBounds(index - first, top);
  const int y = menu_layout::center(index - first, top);
  const bool active = index == s.selected;
  const bool pressed = s.pressedIndex >= 0 && index == unsigned(s.pressedIndex);
  const uint16_t accent = s.page == Page::Routes && index < kRegionCount ? theme::routeColor(regionAt(uint8_t(index)))
                                                                           : accentFor(s.page);
  const uint16_t color = disabled ? theme::kMuted : active ? accent : theme::kText;
  panelFrame(bounds, bounds.height / 2, active, pressed, accent);
  if (active) M5.Display.fillRoundRect(bounds.x + 12, y - 10, 4, 20, 2, accent);
  if (species) thumbnail(species, bounds.x + menu_layout::kIconInset, y);
  const char* trailing = note ? note : carried ? tr(Text::CarryNote) : nullptr;
  M5.Display.setFont(&fonts::efontCN_24);
  M5.Display.setTextColor(color);
  if (!species && !trailing) {
    if (M5.Display.textWidth(label) > bounds.width - 48) M5.Display.setFont(&fonts::efontCN_16);
    M5.Display.setClipRect(bounds.x + 24, y - 24, bounds.width - 48, 48);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(label, 233, y);
    M5.Display.clearClipRect();
    return;
  }
  const int left = species ? bounds.x + 68 : bounds.x + 30;
  const int right = bounds.x + bounds.width - 26;
  int trailingWidth = 0;
  if (trailing) {
    if (M5.Display.textWidth(label) + M5.Display.textWidth(trailing) + 12 > right - left)
      M5.Display.setFont(&fonts::efontCN_16);
    trailingWidth = M5.Display.textWidth(trailing) + 12;
    M5.Display.setTextDatum(middle_right);
    M5.Display.drawString(trailing, right, y);
    M5.Display.setFont(&fonts::efontCN_24);
  }
  if (M5.Display.textWidth(label) > right - left - trailingWidth) M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setClipRect(left, y - 24, right - left - trailingWidth, 48);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString(label, left, y);
  M5.Display.clearClipRect();
}
void Screens::backButton(const ScreenState& s) {
  const bool pressed = s.pressedAux == PressedAux::Back;
  const uint16_t color = pressed ? accentFor(s.page) : theme::kMuted;
  if (pressed) {
    M5.Display.fillRoundRect(103, 44, 19, 17, 7, kPanelPressed);
    M5.Display.drawRoundRect(103, 44, 19, 17, 7, color);
  }
  glyph(theme::Icon::Back, 112, 52, color);
}
void Screens::pagerArrow(const ScreenState& s, bool next) {
  const int y = menu_layout::kPagerY;
  const bool pressed = s.pressedAux == (next ? PressedAux::NextPage : PressedAux::PrevPage);
  const uint16_t color = pressed ? accentFor(s.page) : theme::kMuted;
  if (pressed) {
    const int rx = next ? 280 : 150;
    M5.Display.fillRoundRect(rx, y - 12, 36, 24, 12, kPanelPressed);
    M5.Display.drawRoundRect(rx, y - 12, 36, 24, 12, color);
  }
  glyph(next ? theme::Icon::Next : theme::Icon::Back, next ? 298 : 168, y, color);
}
void Screens::pager(const ScreenState& s, unsigned count) {
  const int y = menu_layout::kPagerY;
  const unsigned pages = menu_layout::pageCount(count), page = s.selected / menu_layout::kPageSize;
  pagerArrow(s, false);
  pagerArrow(s, true);
  if (pages <= menu_layout::kPagerDots) {
    const int first = 233 - int(pages - 1) * 7;
    for (unsigned i = 0; i < pages; ++i)
      M5.Display.fillCircle(first + int(i) * 14, y, i == page ? 4 : 2, i == page ? accentFor(s.page) : theme::kTrack);
    return;
  }
  char text[24]; snprintf(text, sizeof(text), "%u / %u", page + 1, pages);
  M5.Display.setTextDatum(middle_center); M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextColor(theme::kMuted);
  M5.Display.drawString(text, 233, y);
}
} }
