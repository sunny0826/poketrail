#include "screens.hpp"
#include "board.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
#include "icons_generated.hpp"
#include "battery_screen.hpp"
#include <cstdio>
#include <cstdlib>
namespace pokedex { namespace ui {
using namespace adventure;
const char* routeName(Route r) {
  const char* names[] = {"森林", "海边", "洞窟"};
  return uint8_t(r) < 3 ? names[uint8_t(r)] : "未知";
}
const char* speciesName(uint16_t id) {
  return id && id <= data::kSpriteCount ? data::kSprites[id - 1].name : "未选择";
}
unsigned menuCount(const ScreenState& s, const Engine& e) {
  switch (s.page) {
    case Page::Menu: return 9;
    case Page::DateWelcome: return 3;
    case Page::Starter: return 4;
    case Page::Routes: return 4;
    case Page::Partners: {
      unsigned n = 1; for (unsigned id = 1; id <= 151; ++id) n += owns(e.view(), id); return n;
    }
    case Page::Encounter: return 4;
    case Page::Pause: return 2;
    case Page::Throw: return e.throwView().active ? 0 : 3;
    case Page::PartnerDetail: return 2 + e.evolutionView(s.species).count;
    case Page::EvolutionConfirm: case Page::ReleaseConfirm: return 2;
    case Page::Result: return 1;
    case Page::Records: return 5;
    case Page::FirstCaptures: return 152;
    case Page::Recent: return e.view().recentSize + 1;
    case Page::Days: return e.view().historySize + 2;
    case Page::Achievements: return 7;
    case Page::Clock: return 9;
    case Page::Error: return 2;
    default: return 0;
  }
}
void drawGraphicsError(const c152::BatteryStatus& battery) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.drawString("画面不可用，请重启", 233, 233);
  drawBattery(battery);
  M5.Display.endWrite();
}
Screens::~Screens() { free(pixels_); }
bool Screens::begin() {
  pixels_ = static_cast<uint16_t*>(malloc(size_t(kMaxDisplayWidth) * kMaxDisplayHeight * 2));
  return pixels_ != nullptr;
}
void Screens::invalidate() { battlePlayback_.species = resultPlayback_.species = decodedId_ = 0; decodedFrame_ = UINT16_MAX; }
void Screens::line(const char* t, int y, uint16_t color, bool large) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(large ? &fonts::efontCN_24 : &fonts::efontCN_16);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(t, 233, y);
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
  for (unsigned row = 0; row < icon.height; ++row)
    for (unsigned col = 0; col < icon.width; ++col) {
      const uint16_t color = icon.pixels[row * icon.width + col];
      if (color != kTransparentKey)
        M5.Display.fillRect(left + col * scale, top + row * scale, scale, scale, color);
    }
}
void Screens::ballCount(uint64_t count, int y) {
  char text[32]; snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(count));
  M5.Display.setFont(&fonts::efontCN_16);
  const auto& icon = data::kPokeBallIcon;
  const int left = 233 - (int(icon.width) + 6 + M5.Display.textWidth(text)) / 2;
  M5.Display.pushImage(left, y - icon.height / 2, icon.width, icon.height, icon.pixels, kTransparentKey);
  M5.Display.setTextDatum(middle_left); M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(text, left + icon.width + 6, y);
}
void Screens::item(const ScreenState& s, unsigned index, const char* label, int top, uint16_t species, bool carried) {
  const unsigned first = menu_layout::first(s.selected);
  if (index < first || index >= first + menu_layout::kPageSize) return;
  const auto bounds = menu_layout::rowBounds(index - first, top);
  const int y = menu_layout::center(index - first, top);
  const uint16_t background = index == s.selected ? 0x1946 : 0x0821;
  const uint16_t color = index == s.selected ? TFT_CYAN : TFT_WHITE;
  M5.Display.fillRoundRect(bounds.x, bounds.y, bounds.width, bounds.height, 8, background);
  M5.Display.setFont(&fonts::efontCN_24); M5.Display.setTextColor(color, background);
  M5.Display.setTextDatum(species ? middle_left : middle_center);
  M5.Display.drawString(label, species ? 145 : 233, y);
  if (species) thumbnail(species, 119, y);
  if (carried) {
    M5.Display.drawLine(350, y, 355, y + 5, TFT_CYAN);
    M5.Display.drawLine(355, y + 5, 365, y - 6, TFT_CYAN);
  }
}
bool Screens::advance(Playback& playback, uint16_t id, uint32_t now) {
  if (!id || id > data::kSpriteCount) return false;
  const bool changed = playback.species != id;
  if (changed) {
    playback.state = viewer::State(); playback.state.sprite_index = id - 1;
    playback.state.animation_started_ms = now; playback.species = id;
  }
  return viewer::update(playback.state, data::kSprites[id - 1], now) || changed;
}
bool Screens::advanceHome(uint16_t partner, uint32_t now) { return advance(homePlayback_, partner, now); }
bool Screens::advanceResult(uint16_t species, uint32_t now) { return advance(resultPlayback_, species, now); }
bool Screens::sprite(uint16_t id, uint32_t now) { return sprite(id, now, battlePlayback_); }
bool Screens::sprite(uint16_t id, uint32_t now, Playback& playback, int cy) {
  if (!id || id > data::kSpriteCount || !pixels_) return false;
  advance(playback, id, now);
  const auto& s = data::kSprites[id - 1];
  auto scale = displayScale(s);
  if (cy != 233 && scale > 2) scale = 2;
  if (decodedId_ != id || decodedFrame_ != playback.state.frame_index || decodedScale_ != scale) {
    if (!decodeFrame(s, playback.state.frame_index, scale, pixels_, size_t(kMaxDisplayWidth) * kMaxDisplayHeight)) return false;
    decodedId_ = id; decodedFrame_ = playback.state.frame_index; decodedScale_ = scale;
  }
  M5.Display.pushImage(233 - s.width * scale / 2, cy - s.height * scale / 2,
                       s.width * scale, s.height * scale, pixels_, kTransparentKey);
  return true;
}
bool Screens::draw(const ScreenState& s, const Engine& e, const BattleView& practice, uint32_t now) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  bool rendered = pixels_ != nullptr;
  switch (s.page) {
    case Page::Home: rendered = exploration(e, now); break;
    case Page::Battle: case Page::Practice: case Page::Throw:
      battle(s, e, practice, now); break;
    case Page::FirstCaptures: case Page::Recent: case Page::Days: case Page::Achievements:
      records(s, e); break;
    case Page::Clock: clock(s); break;
    case Page::PartnerDetail: case Page::EvolutionConfirm: case Page::ReleaseConfirm:
      progression(s, e); break;
    default: home(s, e, now); break;
  }
  const bool listPage = s.page == Page::Menu || s.page == Page::Clock || s.page == Page::Partners ||
                        s.page == Page::PartnerDetail || s.page == Page::Records;
  if (listPage && menuCount(s, e) > menu_layout::kPageSize) {
    const auto bounds = menu_layout::kNextPage;
    M5.Display.fillRoundRect(bounds.x, bounds.y, bounds.width, bounds.height, 8, 0x0821);
    char count[48]; snprintf(count, sizeof(count), "%u/%u  下一页", s.selected / menu_layout::kPageSize + 1,
                            menu_layout::pageCount(menuCount(s, e)));
    M5.Display.setTextDatum(middle_center); M5.Display.setFont(&fonts::efontCN_16);
    M5.Display.setTextColor(TFT_WHITE, 0x0821);
    M5.Display.drawString(count, 233, bounds.y + bounds.height / 2);
  } else if (s.page != Page::Home && s.page != Page::Battle && s.page != Page::Practice && s.page != Page::Throw)
    line("左 / 右选择 · 双键确认", 422, TFT_DARKGREY);
  drawBattery(s.battery);
  M5.Display.endWrite();
  return rendered;
}
} }
