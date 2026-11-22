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
void label(const char* text, int x, int y, int width, uint16_t color, lgfx::textdatum_t datum = middle_center) {
  M5.Display.setFont(&fonts::efontCN_24);
  if (M5.Display.textWidth(text) > width) M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextDatum(datum);
  M5.Display.setTextColor(color);
  M5.Display.drawString(text, x, y);
}
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
  const bool pressed = s.pressedIndex >= 0 && index == unsigned(s.pressedIndex);
  const uint16_t accent = hubAccent(s.page);
  const uint16_t bg = pressed ? kPanelPressed : active ? theme::kPanelActive : theme::kPanel;
  panelFrame(b, 18, active, pressed, accent);
  const int cx = b.x + b.width / 2;
  const uint16_t color = icon == HubIcon::Region ? theme::routeColor(save.route) : active ? accent : theme::kMuted;
  hubIcon(icon, cx, b.y + b.height * 9 / 25, color, bg, save);
  label(text, cx, b.y + b.height - 22, labelWidth, active ? accent : theme::kText);
}
void Screens::hubExit(const ScreenState& s, const menu_layout::Hub& hub, const char* text, HubIcon icon,
                      const Save& save) {
  const Rect& b = menu_layout::kHubExit;
  const unsigned exitIndex = menu_layout::hubCount(hub) - 1;
  const bool active = s.selected == exitIndex;
  const bool pressed = s.pressedIndex >= 0 && unsigned(s.pressedIndex) == exitIndex;
  const uint16_t color = active ? hubAccent(s.page) : theme::kText;
  const uint16_t bg = pressed ? kPanelPressed : active ? theme::kPanelActive : theme::kPanel;
  panelFrame(b, b.height / 2, active, pressed, hubAccent(s.page));
  M5.Display.setFont(&fonts::efontCN_24);
  if (M5.Display.textWidth(text) > b.width - 64) M5.Display.setFont(&fonts::efontCN_16);
  const int left = b.x + b.width / 2 - (M5.Display.textWidth(text) + 30) / 2;
  hubIcon(icon, left + 11, b.y + b.height / 2, color, bg, save);
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
  panelFrame(b, b.height / 2, s.selected == unsigned(MenuItem::Encounter),
             s.pressedIndex == int16_t(MenuItem::Encounter), theme::kGold);
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
void Screens::drawMainMenu(const ScreenState& s, const Engine& e, bool full) {
  const Save& v = e.view();
  const bool waiting = encounterWaiting(v);
  const bool evolvable = homeEvolutionReady(e);
  const uint16_t pending = pendingEncounterCount(v);
  bool routeEx = false, allEx = false;
  if (!waiting) {
    routeEx = !remainingEncounters(v, v.route);
    if (routeEx) {
      allEx = true;
      for (uint8_t i = 0; i < kRegionCount; ++i) allEx = allEx && !remainingEncounters(v, regionAt(i));
    }
  }
  if (full) {
    mainMenu(s, e);
  } else {
    bool dirty[kMenuItemCount] = {};
    const bool bannerChanged =
        waiting != snap_.encounterWaiting ||
        (waiting ? (v.phase != snap_.phase || pending != snap_.pendingEncounters)
                 : (v.explorationCredit != snap_.explorationCredit ||
                    v.encounterTarget != snap_.encounterTarget ||
                    v.route != snap_.route || routeEx != snap_.routeExhausted ||
                    allEx != snap_.allExhausted));
    if (bannerChanged || (waiting && ((s.selected == 0) != (snap_.selected == 0) ||
                                      (s.pressedIndex == 0) != (snap_.pressedIndex == 0))))
      dirty[0] = true;
    if (v.route != snap_.route) dirty[unsigned(MenuItem::Region)] = true;
    if (v.partner != snap_.partner || evolvable != snap_.homeEvolvable)
      dirty[unsigned(MenuItem::Partner)] = true;
    if (s.selected != snap_.selected) {
      if (snap_.selected < kMenuItemCount) dirty[snap_.selected] = true;
      if (s.selected < kMenuItemCount) dirty[s.selected] = true;
    }
    if (s.pressedIndex != snap_.pressedIndex) {
      if (snap_.pressedIndex > 0 && unsigned(snap_.pressedIndex) < kMenuItemCount)
        dirty[snap_.pressedIndex] = true;
      if (s.pressedIndex > 0 && unsigned(s.pressedIndex) < kMenuItemCount)
        dirty[s.pressedIndex] = true;
    }
    static const Text kLabels[] = {Text::MenuRegion, Text::MenuPartner, Text::MenuPokedex,
                                   Text::MenuRecords, Text::MenuPractice, Text::Settings};
    static const HubIcon kIcons[] = {HubIcon::Region, HubIcon::Partner, HubIcon::Pokedex,
                                     HubIcon::Records, HubIcon::Practice, HubIcon::Settings};
    const char* texts[menu_layout::kMaxHubTiles];
    for (unsigned i = 0; i < menu_layout::kMainHub.tiles; ++i) texts[i] = tr(kLabels[i]);
    const int width = labelWidth(menu_layout::kMainHub, texts);
    auto paint = [&](unsigned idx) {
      const Rect b = menu_layout::hubBounds(menu_layout::kMainHub, idx);
      clearRegion(b.x, b.y, b.width, b.height, theme::kBackground);
      if (idx == 0) {
        encounterBanner(s, v);
      } else if (idx <= menu_layout::kMainHub.tiles) {
        hubTile(s, menu_layout::kMainHub, idx, texts[idx - 1], width, kIcons[idx - 1], v);
        if (idx == unsigned(MenuItem::Partner) && evolvable)
          M5.Display.fillCircle(b.x + b.width - 18, b.y + 16, 5, theme::kGold);
      } else {
        hubExit(s, menu_layout::kMainHub, tr(Text::ReturnExploration), HubIcon::Explore, v);
      }
    };
    const bool row1 = dirty[1] || dirty[2] || dirty[3];
    const bool row2 = dirty[4] || dirty[5] || dirty[6];
    const bool merge01 = dirty[0] && row1;
    const bool merge23 = row2 && dirty[7];
    if (dirty[0] && !merge01) { WriteScope sc; paint(0); }
    if (merge01 || row1 || row2 || merge23) {
      WriteScope sc;
      if (merge01) paint(0);
      for (unsigned i = 1; i <= 6; ++i) if (dirty[i]) paint(i);
      if (merge23) paint(7);
    }
    if (dirty[7] && !merge23) { WriteScope sc; paint(7); }
  }
  snap_.route = v.route;
  snap_.partner = v.partner;
  snap_.homeEvolvable = evolvable;
  snap_.encounterWaiting = waiting;
  snap_.phase = v.phase;
  snap_.pendingEncounters = pending;
  snap_.explorationCredit = v.explorationCredit;
  snap_.encounterTarget = v.encounterTarget;
  snap_.routeExhausted = routeEx;
  snap_.allExhausted = allEx;
}
void Screens::drawRecordsHub(const ScreenState& s, const Engine& e, bool full) {
  const Save& v = e.view();
  if (full) {
    recordsHub(s, e);
  } else {
    if (v.bestDaySteps != snap_.bestDaySteps) {
      WriteScope sc;
      char text[64];
      clearRegion(60, 88, 348, 28, theme::kBackground);
      snprintf(text, sizeof(text), tr(Text::BestDaySteps), static_cast<unsigned long long>(v.bestDaySteps));
      line(text, 102, theme::kGold);
    }
    if (s.selected != snap_.selected || s.pressedIndex != snap_.pressedIndex) {
      bool dirty[5] = {};
      if (snap_.selected < 5) dirty[snap_.selected] = true;
      if (s.selected < 5) dirty[s.selected] = true;
      if (snap_.pressedIndex >= 0 && snap_.pressedIndex < 5) dirty[snap_.pressedIndex] = true;
      if (s.pressedIndex >= 0 && s.pressedIndex < 5) dirty[s.pressedIndex] = true;
      static const Text kLabels[] = {Text::FirstCaptureCards, Text::RecentCaptures, Text::DailySteps, Text::Milestones};
      static const HubIcon kIcons[] = {HubIcon::FirstCaptures, HubIcon::Recent, HubIcon::Days, HubIcon::Milestones};
      const char* texts[4];
      for (unsigned i = 0; i < 4; ++i) texts[i] = tr(kLabels[i]);
      const int width = labelWidth(menu_layout::kRecordsHub, texts);
      auto paint = [&](unsigned idx) {
        const Rect b = menu_layout::hubBounds(menu_layout::kRecordsHub, idx);
        clearRegion(b.x, b.y, b.width, b.height, theme::kBackground);
        if (idx < 4) hubTile(s, menu_layout::kRecordsHub, idx, texts[idx], width, kIcons[idx], v);
        else hubExit(s, menu_layout::kRecordsHub, tr(Text::Back), HubIcon::Back, v);
      };
      if (dirty[0] || dirty[1] || dirty[2] || dirty[3]) {
        WriteScope sc;
        for (unsigned i = 0; i < 4; ++i) if (dirty[i]) paint(i);
      }
      if (dirty[4]) { WriteScope sc; paint(4); }
    }
  }
  snap_.bestDaySteps = v.bestDaySteps;
}
} }
