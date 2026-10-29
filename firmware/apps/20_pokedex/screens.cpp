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
namespace {
uint16_t accentFor(Page page) {
  switch (page) {
    case Page::DateWelcome: case Page::Starter: case Page::Encounter: case Page::Pause:
    case Page::Throw: case Page::Result: case Page::Resources: case Page::Records: case Page::FirstCaptures:
    case Page::Recent: case Page::Days: case Page::Achievements: case Page::EvolutionConfirm:
    case Page::EvolutionPreviewTargets:
      return theme::kGold;
    case Page::Error: case Page::ReleaseConfirm: return theme::kRed;
    case Page::TestEntry: case Page::TestHub: case Page::TestSteps: case Page::TestScenarios:
    case Page::TestFaults: case Page::TestDiagnostics: case Page::TestExit: return theme::kPurple;
    case Page::Clock: return theme::kBlue;
    default: return theme::kMint;
  }
}
}
const char* routeName(Route r) {
  const char* names[] = {"森林", "海边", "洞窟"};
  return uint8_t(r) < 3 ? names[uint8_t(r)] : "未知";
}
const char* speciesName(uint16_t id) {
  return id && id <= data::kSpriteCount ? data::kSprites[id - 1].name : "未选择";
}
unsigned menuCount(const ScreenState& s, const Engine& e) {
  switch (s.page) {
    case Page::Menu: return 8;
    case Page::Resources: return 3;
    case Page::DateWelcome: return 3;
    case Page::Starter: return 4;
    case Page::Routes: return 4;
    case Page::Partners: {
      unsigned n = 1; for (unsigned id = 1; id <= kSpeciesCount; ++id) n += owns(e.view(), id); return n;
    }
    case Page::Encounter: return 4;
    case Page::Pause: return 2;
    case Page::Throw: return e.throwView().active ? 0 : 3;
    case Page::PracticeThrow: return s.capture.active || s.practiceThrowing ? 0 : 3;
    case Page::PracticeResult: return 2;
    case Page::PartnerDetail: return 2 + e.evolutionView(s.species).count;
    case Page::EvolutionConfirm: case Page::ReleaseConfirm: return 2;
    case Page::EvolutionPreviewTargets: return 1 + e.evolutionView(s.evolutionAnim.from).count;
    case Page::Result: return 1;
    case Page::Records: return 5;
    case Page::FirstCaptures: return kSpeciesCount + 1;
    case Page::Recent: return e.view().recentSize + 1;
    case Page::Days: return e.view().historySize + 2;
    case Page::Achievements: return 7;
    case Page::Clock: return 9;
    case Page::Error: return 2;
    case Page::TestEntry: return 3;
    case Page::TestHub: return 8;
    case Page::TestSteps: return 6;
    case Page::TestScenarios: return 5;
    case Page::TestFaults: return 5;
    case Page::TestExit: return 2;
    default: return 0;
  }
}
bool homeEvolutionReady(const Engine& e) {
  if (!e.ready()) return false;
  const EvolutionView view = e.evolutionView(e.view().partner);
  for (unsigned i = 0; i < view.count; ++i)
    if (view.options[i].status == CommandResult::Applied) return true;
  return false;
}
bool touchBackEnabled(Page page) {
  switch (page) {
    case Page::Menu: case Page::Resources: case Page::Routes: case Page::Partners:
    case Page::PartnerDetail: case Page::EvolutionConfirm: case Page::ReleaseConfirm:
    case Page::Records: case Page::FirstCaptures: case Page::Recent: case Page::Days:
    case Page::Achievements: case Page::Clock: case Page::EvolutionPreviewTargets:
    case Page::TestEntry: case Page::TestHub: case Page::TestSteps: case Page::TestScenarios:
    case Page::TestFaults: case Page::TestDiagnostics: case Page::TestExit: return true;
    default: return false;
  }
}
bool isTestPage(Page page) {
  return page == Page::TestEntry || page == Page::TestHub || page == Page::TestSteps ||
         page == Page::TestScenarios || page == Page::TestFaults ||
         page == Page::TestDiagnostics || page == Page::TestExit;
}
void drawGraphicsError(const c152::BatteryStatus& battery) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(233, 233, 227, theme::kBackground);
  M5.Display.drawTriangle(233, 153, 184, 239, 282, 239, theme::kRed);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::efontCN_24);
  M5.Display.setTextColor(theme::kRed);
  M5.Display.drawString("!", 233, 208);
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.drawString("画面不可用，请重启", 233, 290);
  drawBattery(battery);
  M5.Display.endWrite();
}
Screens::~Screens() { free(pixels_); }
bool Screens::begin() {
  pixels_ = static_cast<uint16_t*>(malloc(size_t(kMaxDisplayWidth) * kMaxDisplayHeight * 2));
  return pixels_ != nullptr;
}
void Screens::invalidate() {
  battlePlayback_.species = enemyBattlePlayback_.species = resultPlayback_.species = decodedId_ = 0;
  decodedFrame_ = UINT16_MAX; decodedSilhouette_ = 0;
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
void Screens::ballIcon(int y, int x) {
  const auto& icon = data::kPokeBallIcon;
  M5.Display.pushImage(x - icon.width / 2, y - icon.height / 2, icon.width, icon.height,
                       icon.pixels, kTransparentKey);
}
void Screens::item(const ScreenState& s, unsigned index, const char* label, int top,
                   uint16_t species, bool carried, const char* note) {
  const unsigned first = menu_layout::first(s.selected);
  if (index < first || index >= first + menu_layout::kPageSize) return;
  const auto bounds = menu_layout::rowBounds(index - first, top);
  const int y = menu_layout::center(index - first, top);
  const bool active = index == s.selected;
  const uint16_t accent = accentFor(s.page);
  const uint16_t background = active ? theme::kPanelActive : theme::kPanel;
  const uint16_t color = active ? accent : theme::kText;
  M5.Display.fillRoundRect(bounds.x, bounds.y, bounds.width, bounds.height, 16, background);
  if (active) {
    M5.Display.drawRoundRect(bounds.x, bounds.y, bounds.width, bounds.height, 16, accent);
    M5.Display.fillRect(bounds.x + 1, y - 12, 4, 24, accent);
  }
  if (species) thumbnail(species, bounds.x + 34, y);
  const char* trailing = note ? note : carried ? "携带" : nullptr;
  M5.Display.setFont(&fonts::efontCN_24);
  M5.Display.setTextColor(color);
  if (!species && !trailing) {
    M5.Display.setClipRect(bounds.x + 12, y - 24, bounds.width - 24, 48);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(label, 233, y);
    M5.Display.clearClipRect();
    return;
  }
  const int left = species ? bounds.x + 64 : bounds.x + 16;
  const int right = bounds.x + bounds.width - 16;
  const int trailingWidth = trailing ? M5.Display.textWidth(trailing) + 12 : 0;
  M5.Display.setClipRect(left, y - 24, right - left - trailingWidth, 48);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString(label, left, y);
  M5.Display.clearClipRect();
  if (trailing) {
    M5.Display.setTextDatum(middle_right);
    M5.Display.drawString(trailing, right, y);
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
bool Screens::advanceScene(Route route, uint32_t now, bool active) {
  if (sceneRoute_ != route || !sceneTiming_) {
    sceneRoute_ = route;
    sceneSteps_ = 0;
    sceneLastMs_ = now;
    sceneTiming_ = true;
    return true;
  }
  if (!active) {
    sceneLastMs_ = now;
    return false;
  }
  const uint32_t elapsed = now - sceneLastMs_;
  if (elapsed < 100) return false;
  const uint32_t steps = elapsed / 100;
  sceneLastMs_ += steps * 100;
  sceneSteps_ += steps;
  return true;
}
bool Screens::sprite(uint16_t id, uint32_t now) { return sprite(id, now, battlePlayback_); }
bool Screens::sprite(uint16_t id, uint32_t now, Playback& playback, int cy, int cx,
                     uint16_t silhouette, uint8_t maxScale, uint16_t maxHeight) {
  if (!id || id > data::kSpriteCount || !pixels_ || !maxScale) return false;
  advance(playback, id, now);
  const auto& s = data::kSprites[id - 1];
  auto scale = displayScale(s);
  if (scale > maxScale) scale = maxScale;
  while (scale > 1 && s.height * scale > maxHeight) --scale;
  if (decodedId_ != id || decodedFrame_ != playback.state.frame_index ||
      decodedScale_ != scale || decodedSilhouette_ != silhouette) {
    if (!decodeFrame(s, playback.state.frame_index, scale, pixels_, size_t(kMaxDisplayWidth) * kMaxDisplayHeight)) return false;
    if (silhouette) {
      const size_t count = size_t(s.width) * scale * s.height * scale;
      for (size_t index = 0; index < count; ++index)
        if (pixels_[index] != kTransparentKey) pixels_[index] = silhouette;
    }
    decodedId_ = id; decodedFrame_ = playback.state.frame_index; decodedScale_ = scale;
    decodedSilhouette_ = silhouette;
  }
  M5.Display.pushImage(cx - s.width * scale / 2, cy - s.height * scale / 2,
                       s.width * scale, s.height * scale, pixels_, kTransparentKey);
  return true;
}
bool Screens::draw(const ScreenState& s, const Engine& e, const BattleView& practice, uint32_t now) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(233, 233, 227, theme::kBackground);
  bool rendered = pixels_ != nullptr;
  switch (s.page) {
    case Page::Home: rendered = exploration(e, now); break;
    case Page::Battle: case Page::Practice: case Page::Throw:
      battle(s, e, practice, now); break;
    case Page::PracticeThrow: practiceThrow(s, now); break;
    case Page::PracticeResult: practiceResult(s, now); break;
    case Page::Evolution: case Page::EvolutionPreview: rendered = evolution(s, now); break;
    case Page::EvolutionPreviewTargets: evolutionPreviewTargets(s, e); break;
    case Page::FirstCaptures: case Page::Recent: case Page::Days: case Page::Achievements:
      records(s, e); break;
    case Page::Clock: clock(s); break;
    case Page::TestEntry: testEntry(s); break;
    case Page::TestHub: testHub(s, e); break;
    case Page::TestSteps: testSteps(s); break;
    case Page::TestScenarios: testScenarios(s); break;
    case Page::TestFaults: testFaults(s); break;
    case Page::TestDiagnostics: testDiagnostics(s, e); break;
    case Page::TestExit: testExit(s); break;
    case Page::PartnerDetail: case Page::EvolutionConfirm: case Page::ReleaseConfirm:
      progression(s, e); break;
    default: home(s, e, now); break;
  }
  const bool listPage = s.page == Page::Menu || s.page == Page::Resources || s.page == Page::Clock ||
                        s.page == Page::Partners || s.page == Page::PartnerDetail || s.page == Page::Records ||
                        s.page == Page::EvolutionPreviewTargets || s.page == Page::TestHub ||
                        s.page == Page::TestSteps || s.page == Page::TestScenarios ||
                        s.page == Page::TestFaults;
  if (listPage && menuCount(s, e) > menu_layout::kPageSize) {
    M5.Display.fillCircle(168, 430, 20, theme::kPanel);
    M5.Display.fillCircle(298, 430, 20, theme::kPanel);
    glyph(theme::Icon::Back, 168, 430, theme::kText);
    glyph(theme::Icon::Next, 298, 430, theme::kText);
    char count[24]; snprintf(count, sizeof(count), "%u / %u", s.selected / menu_layout::kPageSize + 1,
                            menu_layout::pageCount(menuCount(s, e)));
    M5.Display.setTextDatum(middle_center); M5.Display.setFont(&fonts::efontCN_24);
    M5.Display.setTextColor(theme::kText);
    M5.Display.drawString(count, 233, 430);
  }
  if (touchBackEnabled(s.page)) {
    glyph(theme::Icon::Back, 112, 52, theme::kMuted);
    if (s.page == Page::Menu) {
      M5.Display.setTextDatum(middle_left); M5.Display.setFont(&fonts::efontCN_16);
      M5.Display.setTextColor(theme::kMuted);
      M5.Display.drawString("首页", 128, 52);
    }
  }
  drawBattery(s.battery);
  if (s.testMode) {
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::efontCN_16);
    M5.Display.setTextColor(theme::kGold);
    M5.Display.drawString("测试", 233, 452);
  }
  M5.Display.endWrite();
  return rendered;
}
} }
