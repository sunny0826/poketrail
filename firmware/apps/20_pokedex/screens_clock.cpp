#include "screens.hpp"
#include "board.hpp"
#include "sprites_generated.hpp"
#include "sprite.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using i18n::Text;
menu_layout::Rect Screens::spriteBounds(uint16_t id, int cx, int cy, uint8_t maxScale, uint16_t maxHeight) const {
  if (!id || id > data::kSpriteCount || !maxScale) return {0, 0, 0, 0};
  const auto& s = data::kSprites[id - 1];
  uint16_t scale = displayScale(s);
  if (scale > maxScale) scale = maxScale;
  while (scale > 1 && s.height * scale > maxHeight) --scale;
  const int w = int(s.width) * int(scale), h = int(s.height) * int(scale);
  return {cx - w / 2, cy - h / 2, w, h};
}
void Screens::updateListRows(const ScreenState& s, const adventure::Engine& e, int top, bool pagerOn,
                             unsigned count, RowDrawer drawer, bool hasScene,
                             adventure::Route sceneRoute, bool dim) {
  if (s.selected == snap_.selected && s.pressedIndex == snap_.pressedIndex) return;
  const unsigned oldFirst = menu_layout::first(snap_.selected);
  const unsigned newFirst = menu_layout::first(s.selected);
  WriteScope scope;
  if (oldFirst == newFirst) {
    for (unsigned i = newFirst; i < count && i < newFirst + menu_layout::kPageSize; ++i) {
      const bool activeChanged = (i == snap_.selected) != (i == s.selected);
      const bool pressChanged =
          (snap_.pressedIndex >= 0 && i == unsigned(snap_.pressedIndex)) !=
          (s.pressedIndex >= 0 && i == unsigned(s.pressedIndex));
      if (activeChanged || pressChanged) {
        const auto b = menu_layout::rowBounds(i - newFirst, top);
        restoreBackdrop(b.x, b.y, b.width, b.height, sceneRoute, dim, hasScene);
        (this->*drawer)(s, e, i, top);
      }
    }
    return;
  }
  restoreBackdrop(72, top - 30, 322, 2 * menu_layout::kPitch + 60, sceneRoute, dim, hasScene);
  for (unsigned i = newFirst; i < count && i < newFirst + menu_layout::kPageSize; ++i)
    (this->*drawer)(s, e, i, top);
  if (pagerOn && count > menu_layout::kPageSize) {
    clearRegion(150, menu_layout::kPagerY - 12, 166, 24, theme::kBackground);
    pager(s, count);
  }
}
void Screens::drawTitleCleared(const ScreenState& s, const char* heading, const char* subtitle,
                               uint16_t accent, bool clearSubtitle) {
  clearRegion(100, 36, 268, 30, theme::kBackground);
  if (clearSubtitle) clearRegion(70, 68, 328, 22, theme::kBackground);
  title(heading, subtitle, accent);
  if (touchBackEnabled(s.page)) backButton(s);
}
void Screens::clock(const ScreenState& s) {
  char text[90];
  title(s.editing ? tr(Text::EditDateTime) : tr(Text::DateTime), "", theme::kBlue);
  snprintf(text, sizeof(text), "%04d-%02d-%02d  %02d:%02d", s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute);
  line(text, 80, theme::kBlue);
  if (s.clockError) alert(tr(Text::ClockError), 169, theme::kRed);
  else if (s.clockNeedsSync && !s.editing) line(tr(Text::ClockDraft), 157, theme::kGold);
  else if (s.editing) line(tr(Text::ClockKeys), 157, theme::kMuted);
  const char* names[] = {tr(Text::Year), tr(Text::Month), tr(Text::Day), tr(Text::Hour), tr(Text::Minute), tr(Text::TimezoneMinutes)};
  int values[] = {s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute, s.offset};
  const char* modes[] = {tr(Text::ClockNormal), tr(Text::ClockTimezone), tr(Text::ClockCorrect)};
  for (unsigned i = 0; i < 9; ++i) {
    if (i < 6) {
      snprintf(text, sizeof(text), "%d", values[i]);
      item(s, i, names[i], menu_layout::kTop, 0, false, text);
    } else if (i == 6) item(s, i, tr(Text::ClockMode), menu_layout::kTop, 0, false, modes[uint8_t(s.clockMode)]);
    else item(s, i, i == 7 ? tr(Text::SaveClock) : tr(Text::Back), menu_layout::kTop);
  }
}
void Screens::drawClockRow(const ScreenState& s, const adventure::Engine&, unsigned i, int top) {
  char text[32];
  const char* names[] = {tr(Text::Year), tr(Text::Month), tr(Text::Day), tr(Text::Hour), tr(Text::Minute), tr(Text::TimezoneMinutes)};
  int values[] = {s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute, s.offset};
  const char* modes[] = {tr(Text::ClockNormal), tr(Text::ClockTimezone), tr(Text::ClockCorrect)};
  if (i < 6) {
    snprintf(text, sizeof(text), "%d", values[i]);
    item(s, i, names[i], top, 0, false, text);
  } else if (i == 6) {
    item(s, i, tr(Text::ClockMode), top, 0, false, modes[uint8_t(s.clockMode)]);
  } else if (i == 7) {
    item(s, i, tr(Text::SaveClock), top);
  } else {
    item(s, i, tr(Text::Back), top);
  }
}
void Screens::drawClock(const ScreenState& s, const adventure::Engine& e, bool full) {
  if (full) {
    clock(s);
  } else {
    if (s.editing != snap_.editing) {
      WriteScope sc;
      drawTitleCleared(s, s.editing ? tr(Text::EditDateTime) : tr(Text::DateTime), "", theme::kBlue, false);
    }
    const bool timeChanged = s.civil.year != snap_.civil.year || s.civil.month != snap_.civil.month ||
                             s.civil.day != snap_.civil.day || s.civil.hour != snap_.civil.hour ||
                             s.civil.minute != snap_.civil.minute;
    if (timeChanged) {
      WriteScope sc;
      char text[90];
      clearRegion(100, 68, 268, 24, theme::kBackground);
      snprintf(text, sizeof(text), "%04d-%02d-%02d  %02d:%02d", s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute);
      line(text, 80, theme::kBlue);
    }
    if (s.clockError != snap_.clockError || s.editing != snap_.editing || s.clockNeedsSync != snap_.clockNeedsSync) {
      WriteScope sc;
      clearRegion(60, 145, 348, 42, theme::kBackground);
      if (s.clockError) alert(tr(Text::ClockError), 169, theme::kRed);
      else if (s.clockNeedsSync && !s.editing) line(tr(Text::ClockDraft), 157, theme::kGold);
      else if (s.editing) line(tr(Text::ClockKeys), 157, theme::kMuted);
    }
    const unsigned oldFirst = menu_layout::first(snap_.selected);
    const unsigned newFirst = menu_layout::first(s.selected);
    if (oldFirst == newFirst) {
      const bool changed[] = {
        s.civil.year != snap_.civil.year, s.civil.month != snap_.civil.month,
        s.civil.day != snap_.civil.day, s.civil.hour != snap_.civil.hour,
        s.civil.minute != snap_.civil.minute, s.offset != snap_.offset,
        s.clockMode != snap_.clockMode, false, false
      };
      for (unsigned i = newFirst; i < 9 && i < newFirst + menu_layout::kPageSize; ++i) {
        if (changed[i] && i != s.selected && i != snap_.selected) {
          WriteScope sc;
          const auto b = menu_layout::rowBounds(i - newFirst, menu_layout::kTop);
          clearRegion(b.x, b.y, b.width, b.height, theme::kBackground);
          drawClockRow(s, e, i, menu_layout::kTop);
        }
      }
      if (s.selected == snap_.selected && s.selected < 9 && changed[s.selected]) {
        WriteScope sc;
        const auto b = menu_layout::rowBounds(s.selected - newFirst, menu_layout::kTop);
        clearRegion(b.x, b.y, b.width, b.height, theme::kBackground);
        drawClockRow(s, e, s.selected, menu_layout::kTop);
      }
    }
    updateListRows(s, e, menu_layout::kTop, true, 9, &Screens::drawClockRow);
  }
  snap_.clockError = s.clockError;
  snap_.clockNeedsSync = s.clockNeedsSync;
  snap_.civil = s.civil;
  snap_.offset = s.offset;
  snap_.clockMode = s.clockMode;
}
} }
