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
using i18n::Text;
namespace {
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
const char* routeName(i18n::Language language, Route r) {
  const Text names[] = {Text::RouteForest, Text::RouteSea, Text::RouteCave, Text::RouteKanto,
                        Text::RouteJohto, Text::RouteHoenn, Text::RouteSinnoh, Text::RouteUnova};
  return i18n::text(language, r <= Route::Unova ? names[uint8_t(r)] : Text::Unknown);
}
unsigned menuCount(const ScreenState& s, const Engine& e) {
  switch (s.page) {
    case Page::Menu: return kMenuItemCount;
    case Page::CarriedPartner: return e.view().partner ? 2 + e.evolutionView(e.view().partner).count : 1;
    case Page::Settings: return 3;
    case Page::DateWelcome: return 4;
    case Page::Starter: return 4;
    case Page::Routes: return kRegionCount + 1;
    case Page::Partners: {
      unsigned n = 1; for (unsigned id = 1; id <= kSpeciesCount; ++id) n += owns(e.view(), id); return n;
    }
    case Page::Encounter: return 4;
    case Page::Pause: return 2;
    case Page::Throw: {
      const auto& v = e.view();
      // Only a legacy encounter whose ball stock is empty needs a way out:
      // the engine refuses to open a window without balls.
      return (v.encounter.ruleVersion < 3 && !availableBalls(v)) ? 1 : 0;
    }
    case Page::PracticeThrow: return 0;
    case Page::PracticeResult: return 2;
    case Page::PartnerDetail: return 2 + e.evolutionView(s.species).count;
    case Page::EvolutionConfirm: return 2;
    case Page::EvolutionPreviewTargets: return 1 + e.evolutionView(s.evolutionAnim.from).count;
    case Page::Result: return 1;
    case Page::Records: return 5;
    case Page::FirstCaptures: return kSpeciesCount + 1;
    case Page::Recent: return e.view().recentSize + 1;
    case Page::Days: return e.view().historySize + 2;
    case Page::Achievements: return 7;
    case Page::Clock: return 9;
    case Page::Error: return 3;
    case Page::Language: return 3;
    case Page::TestEntry: return 3;
    case Page::TestHub: return 8;
    case Page::TestSteps: return 6;
    case Page::TestScenarios: return 5;
    case Page::TestFaults: return 5;
    case Page::TestExit: return 2;
    default: return 0;
  }
}
int menuTop(const ScreenState& s, const Engine& e) {
  if (s.page == Page::Result)
    return e.view().result == Result::Captured ? menu_layout::kCaptureResultTop : menu_layout::kResultTop;
  return menu_layout::kTop;
}
const menu_layout::Hub* hubFor(Page page) {
  if (page == Page::Menu) return &menu_layout::kMainHub;
  if (page == Page::Records) return &menu_layout::kRecordsHub;
  return nullptr;
}
bool menuSelectable(const ScreenState& s, const Engine& e, unsigned index) {
  return s.page != Page::Menu || index != unsigned(MenuItem::Encounter) || encounterWaiting(e.view());
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
    case Page::Menu: case Page::Settings: case Page::Routes: case Page::Partners:
    case Page::CarriedPartner: case Page::PartnerDetail: case Page::EvolutionConfirm:
    case Page::Records: case Page::FirstCaptures: case Page::Recent: case Page::Days:
    case Page::Achievements: case Page::Clock: case Page::EvolutionPreviewTargets:
    case Page::TestEntry: case Page::TestHub: case Page::TestSteps: case Page::TestScenarios:
    case Page::TestFaults: case Page::TestDiagnostics: case Page::TestExit:
    case Page::Language: return true;
    default: return false;
  }
}
bool isTestPage(Page page) {
  return page == Page::TestEntry || page == Page::TestHub || page == Page::TestSteps ||
         page == Page::TestScenarios || page == Page::TestFaults ||
         page == Page::TestDiagnostics || page == Page::TestExit;
}
void drawGraphicsError(const c152::BatteryStatus& battery, i18n::Language language) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(233, 233, 227, theme::kBackground);
  M5.Display.drawTriangle(233, 153, 184, 239, 282, 239, theme::kRed);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::efontCN_24);
  M5.Display.setTextColor(theme::kRed);
  M5.Display.drawString("!", 233, 208);
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.drawString(i18n::text(language, Text::GraphicsError), 233, 290);
  drawBattery(battery);
  M5.Display.endWrite();
}
Screens::~Screens() { free(pixels_); }
bool Screens::begin() {
  if (!pixels_) pixels_ = static_cast<uint16_t*>(malloc(size_t(kMaxDisplayWidth) * kMaxDisplayHeight * 2));
  return pixels_ != nullptr;
}
bool Screens::startup(uint32_t elapsed) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(233, 233, 227, theme::kBackground);
  M5.Display.fillCircle(233, 207, 91, theme::kPanel);
  M5.Display.fillEllipse(233, 259, 45, 9, theme::kBackground);
  // The brand illustration uses 3x pixels without changing gameplay scaling.
  advance(startupPlayback_, 1, elapsed);
  const auto& sprite = data::kSprites[0];
  const bool rendered = decodeFrame(sprite, startupPlayback_.state.frame_index, 3,
                                    pixels_, size_t(kMaxDisplayWidth) * kMaxDisplayHeight);
  if (rendered) M5.Display.pushImage(233 - sprite.width * 3 / 2, 207 - sprite.height * 3 / 2,
                                   sprite.width * 3, sprite.height * 3, pixels_, kTransparentKey);
  decodedId_ = 0;  // The shared pixel buffer no longer contains the previous page's sprite.
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(theme::kText);
  M5.Display.drawString("PokeTrail", 233, 333);
  M5.Display.setTextSize(1);
  M5.Display.endWrite();
  return rendered;
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
void Screens::item(const ScreenState& s, unsigned index, const char* label, int top,
                   uint16_t species, bool carried, const char* note, bool disabled) {
  const unsigned first = menu_layout::first(s.selected);
  if (index < first || index >= first + menu_layout::kPageSize) return;
  const auto bounds = menu_layout::rowBounds(index - first, top);
  const int y = menu_layout::center(index - first, top);
  const bool active = index == s.selected;
  const uint16_t accent = s.page == Page::Routes && index < kRegionCount ? theme::routeColor(regionAt(uint8_t(index)))
                                                                           : accentFor(s.page);
  const uint16_t background = active ? theme::kPanelActive : theme::kPanel;
  const uint16_t color = disabled ? theme::kMuted : active ? accent : theme::kText;
  const int radius = bounds.height / 2;
  M5.Display.fillRoundRect(bounds.x, bounds.y, bounds.width, bounds.height, radius, background);
  if (active) {
    M5.Display.drawRoundRect(bounds.x, bounds.y, bounds.width, bounds.height, radius, accent);
    M5.Display.drawRoundRect(bounds.x + 1, bounds.y + 1, bounds.width - 2, bounds.height - 2, radius - 1, accent);
    M5.Display.fillRoundRect(bounds.x + 12, y - 10, 4, 20, 2, accent);
  }
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
void Screens::pager(const ScreenState& s, unsigned count) {
  const int y = menu_layout::kPagerY;
  const unsigned pages = menu_layout::pageCount(count), page = s.selected / menu_layout::kPageSize;
  glyph(theme::Icon::Back, 168, y, theme::kMuted);
  glyph(theme::Icon::Next, 298, y, theme::kMuted);
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
  language_ = s.language;
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
    case Page::Language: language(s, e); break;
    case Page::TestEntry: testEntry(s); break;
    case Page::TestHub: testHub(s, e); break;
    case Page::TestSteps: testSteps(s); break;
    case Page::TestScenarios: testScenarios(s); break;
    case Page::TestFaults: testFaults(s); break;
    case Page::TestDiagnostics: testDiagnostics(s, e); break;
    case Page::TestExit: testExit(s); break;
    case Page::CarriedPartner: case Page::PartnerDetail: case Page::EvolutionConfirm:
      progression(s, e); break;
    case Page::Menu: mainMenu(s, e); break;
    case Page::Records: recordsHub(s, e); break;
    default: home(s, e, now); break;
  }
  const bool listPage = s.page == Page::DateWelcome || s.page == Page::Settings || s.page == Page::Clock ||
                        s.page == Page::Partners || s.page == Page::PartnerDetail || s.page == Page::CarriedPartner ||
                        s.page == Page::EvolutionPreviewTargets || s.page == Page::TestHub ||
                        s.page == Page::TestSteps || s.page == Page::TestScenarios ||
                        s.page == Page::TestFaults;
  if (listPage && menuCount(s, e) > menu_layout::kPageSize) pager(s, menuCount(s, e));
  if (touchBackEnabled(s.page)) glyph(theme::Icon::Back, 112, 52, theme::kMuted);
  drawBattery(s.battery);
  if (s.testMode) {
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::efontCN_16);
    M5.Display.setTextColor(theme::kGold);
    M5.Display.drawString(tr(Text::TestBadge), 233, 452);
  }
  M5.Display.endWrite();
  return rendered;
}
} }
