#include "screens.hpp"
#include "board.hpp"
#include "encounter_catalog.hpp"
#include "icons_generated.hpp"
#include "sprite.hpp"
#include <cmath>
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
using theme::HubIcon;
namespace {
using menu_layout::Rect;
uint16_t hubAccent(Page page) { return page == Page::Records ? theme::kGold : theme::kMint; }
void frame(const Rect& b, int radius, bool active, uint16_t accent) {
  M5.Display.fillRoundRect(b.x, b.y, b.width, b.height, radius, active ? theme::kPanelActive : theme::kPanel);
  if (!active) return;
  M5.Display.drawRoundRect(b.x, b.y, b.width, b.height, radius, accent);
  M5.Display.drawRoundRect(b.x + 1, b.y + 1, b.width - 2, b.height - 2, radius - 1, accent);
}
void label(const char* text, int x, int y, int width, uint16_t color, lgfx::textdatum_t datum = middle_center) {
  M5.Display.setFont(&fonts::efontCN_24);
  if (M5.Display.textWidth(text) > width) M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextDatum(datum);
  M5.Display.setTextColor(color);
  M5.Display.drawString(text, x, y);
}
// One label size per hub keeps neighbouring tiles visually aligned.
int labelWidth(const menu_layout::Hub& hub, const char* const* labels) {
  M5.Display.setFont(&fonts::efontCN_24);
  for (unsigned i = 0; i < hub.tiles; ++i)
    if (M5.Display.textWidth(labels[i]) > hub.width - 16) return 0;
  return hub.width - 16;
}
}
void Screens::hubIcon(HubIcon icon, int x, int y, uint16_t color, uint16_t background, const Save& save) {
  auto& d = M5.Display;
  switch (icon) {
    case HubIcon::Region:
      d.fillTriangle(x - 2, y + 12, x + 9, y - 5, x + 20, y + 12, color);
      d.fillTriangle(x - 23, y + 12, x - 6, y - 14, x + 11, y + 12, background);
      d.fillTriangle(x - 19, y + 12, x - 6, y - 9, x + 7, y + 12, color);
      d.fillCircle(x + 14, y - 12, 4, color);
      break;
    case HubIcon::Partner:
      if (save.partner) thumbnail(save.partner, x, y, 2);
      else d.fillCircle(x, y, 12, color);
      break;
    case HubIcon::Pokedex:
      d.fillRoundRect(x - 18, y - 13, 17, 27, 3, color);
      d.fillRoundRect(x + 1, y - 13, 17, 27, 3, color);
      for (int dy : {-6, 0, 6}) {
        d.fillRect(x - 14, y + dy, 9, 2, background);
        d.fillRect(x + 5, y + dy, 9, 2, background);
      }
      break;
    case HubIcon::Records:
      for (int r : {4, 5}) { d.drawCircle(x - 12, y - 7, r, color); d.drawCircle(x + 12, y - 7, r, color); }
      d.fillRoundRect(x - 11, y - 14, 22, 18, 7, color);
      d.fillRect(x - 11, y - 14, 22, 6, color);
      d.fillRect(x - 2, y + 3, 5, 7, color);
      d.fillRoundRect(x - 9, y + 9, 19, 5, 2, color);
      break;
    case HubIcon::Practice:
      d.fillCircle(x, y, 16, color); d.fillCircle(x, y, 12, background);
      d.fillCircle(x, y, 7, color); d.fillCircle(x, y, 3, background);
      break;
    case HubIcon::Settings:
      for (unsigned k = 0; k < 8; ++k) {
        const float a = float(k) * 0.7853982f;
        d.fillCircle(x + int(std::lround(13 * std::cos(a))), y + int(std::lround(13 * std::sin(a))), 4, color);
      }
      d.fillCircle(x, y, 12, color); d.fillCircle(x, y, 5, background);
      break;
    case HubIcon::Explore:
      d.fillTriangle(x - 11, y - 1, x, y - 11, x + 11, y - 1, color);
      d.fillRect(x - 7, y - 1, 14, 10, color);
      d.fillRect(x - 2, y + 3, 4, 6, background);
      break;
    case HubIcon::Back: glyph(theme::Icon::Back, x, y, color); break;
    case HubIcon::FirstCaptures: captureBadge(save, x - 14, y); break;
    case HubIcon::Recent:
      d.fillCircle(x, y, 15, color); d.fillCircle(x, y, 12, background);
      d.fillRect(x - 1, y - 9, 3, 10, color); d.fillRect(x - 1, y - 1, 9, 3, color);
      break;
    case HubIcon::Days:
      d.fillRoundRect(x - 16, y + 1, 8, 13, 2, color);
      d.fillRoundRect(x - 4, y - 6, 8, 20, 2, color);
      d.fillRoundRect(x + 8, y - 14, 8, 28, 2, color);
      break;
    case HubIcon::Milestones:
      d.fillTriangle(x - 11, y - 16, x - 3, y - 16, x + 1, y - 2, color);
      d.fillTriangle(x + 3, y - 16, x + 11, y - 16, x - 1, y - 2, color);
      d.fillCircle(x, y + 4, 11, color); d.fillCircle(x, y + 4, 6, background);
      d.fillCircle(x, y + 4, 2, color);
      break;
  }
}
void Screens::hubTiles(const ScreenState& s, const menu_layout::Hub& hub, const Text* labels,
                       const HubIcon* icons, const Save& save) {
  const char* text[menu_layout::kMaxHubTiles];
  for (unsigned i = 0; i < hub.tiles; ++i) text[i] = tr(labels[i]);
  const int width = labelWidth(hub, text);
  for (unsigned i = 0; i < hub.tiles; ++i) hubTile(s, hub, i + (hub.banner ? 1 : 0), text[i], width, icons[i], save);
}
void Screens::hubTile(const ScreenState& s, const menu_layout::Hub& hub, unsigned index, const char* text,
                      int labelWidth, HubIcon icon, const Save& save) {
  const Rect b = menu_layout::hubBounds(hub, index);
  const bool active = s.selected == index;
  const uint16_t accent = hubAccent(s.page);
  frame(b, 18, active, accent);
  const int cx = b.x + b.width / 2;
  const uint16_t color = icon == HubIcon::Region ? theme::routeColor(save.route) : active ? accent : theme::kMuted;
  hubIcon(icon, cx, b.y + b.height * 9 / 25, color, active ? theme::kPanelActive : theme::kPanel, save);
  label(text, cx, b.y + b.height - 22, labelWidth, active ? accent : theme::kText);
}
void Screens::hubExit(const ScreenState& s, const menu_layout::Hub& hub, const char* text, HubIcon icon,
                      const Save& save) {
  const Rect& b = menu_layout::kHubExit;
  const bool active = s.selected == menu_layout::hubCount(hub) - 1;
  const uint16_t color = active ? hubAccent(s.page) : theme::kText;
  frame(b, b.height / 2, active, hubAccent(s.page));
  M5.Display.setFont(&fonts::efontCN_24);
  if (M5.Display.textWidth(text) > b.width - 64) M5.Display.setFont(&fonts::efontCN_16);
  const int left = b.x + b.width / 2 - (M5.Display.textWidth(text) + 30) / 2;
  hubIcon(icon, left + 11, b.y + b.height / 2, color, active ? theme::kPanelActive : theme::kPanel, save);
  label(text, left + 30, b.y + b.height / 2, b.width - 64, color, middle_left);
}
void Screens::encounterBanner(const ScreenState& s, const Save& v) {
  const Rect& b = menu_layout::kHubBanner;
  const int cx = b.x + b.width / 2, cy = b.y + b.height / 2;
  char text[64];
  if (!encounterWaiting(v)) {
    if (!remainingEncounters(v, v.route)) {
      bool all = true;
      for (uint8_t i = 0; i < kRegionCount; ++i) all = all && !remainingEncounters(v, regionAt(i));
      fitLine(all ? tr(Text::AllCaught) : tr(Text::RegionCaught), cy, b.width - 16, theme::kGold, false);
      return;
    }
    snprintf(text, sizeof(text), tr(Text::NextEncounterSteps), static_cast<unsigned long long>(
      v.explorationCredit < v.encounterTarget ? v.encounterTarget - v.explorationCredit : 0));
    fitLine(text, cy - 10, b.width - 16, theme::kMuted, false);
    progress(v.explorationCredit, v.encounterTarget, cy + 10, theme::routeColor(v.route));
    return;
  }
  frame(b, b.height / 2, s.selected == unsigned(MenuItem::Encounter), theme::kGold);
  const auto& ball = data::kPokeBallIcon;
  M5.Display.pushImage(b.x + 34 - ball.width / 2, cy - ball.height / 2, ball.width, ball.height,
                       ball.pixels, kTransparentKey);
  if (v.phase == Phase::ThrowReady) snprintf(text, sizeof(text), "%s", tr(Text::ResumeCatch));
  else snprintf(text, sizeof(text), tr(Text::ResumeEncounterCount), pendingEncounterCount(v));
  label(text, cx + 14, cy, b.width - 90, theme::kGold);
}
void Screens::mainMenu(const ScreenState& s, const Engine& e) {
  const Save& v = e.view();
  encounterBanner(s, v);
  const Text labels[] = {Text::MenuRegion, Text::MenuPartner, Text::MenuPokedex,
                         Text::MenuRecords, Text::MenuPractice, Text::Settings};
  const HubIcon icons[] = {HubIcon::Region, HubIcon::Partner, HubIcon::Pokedex,
                           HubIcon::Records, HubIcon::Practice, HubIcon::Settings};
  hubTiles(s, menu_layout::kMainHub, labels, icons, v);
  if (homeEvolutionReady(e)) {
    const Rect b = menu_layout::hubBounds(menu_layout::kMainHub, unsigned(MenuItem::Partner));
    M5.Display.fillCircle(b.x + b.width - 18, b.y + 16, 5, theme::kGold);
  }
  hubExit(s, menu_layout::kMainHub, tr(Text::ReturnExploration), HubIcon::Explore, v);
}
void Screens::recordsHub(const ScreenState& s, const Engine& e) {
  const Save& v = e.view();
  char text[64];
  title(tr(Text::MenuRecords), "", theme::kGold);
  snprintf(text, sizeof(text), tr(Text::BestDaySteps), static_cast<unsigned long long>(v.bestDaySteps));
  line(text, 102, theme::kGold);
  const Text labels[] = {Text::FirstCaptureCards, Text::RecentCaptures, Text::DailySteps, Text::Milestones};
  const HubIcon icons[] = {HubIcon::FirstCaptures, HubIcon::Recent, HubIcon::Days, HubIcon::Milestones};
  hubTiles(s, menu_layout::kRecordsHub, labels, icons, v);
  hubExit(s, menu_layout::kRecordsHub, tr(Text::Back), HubIcon::Back, v);
}
} }
