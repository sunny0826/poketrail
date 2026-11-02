#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using i18n::Text;
void Screens::clock(const ScreenState& s) {
  char text[90];
  title(s.editing ? tr(Text::EditDateTime) : tr(Text::DateTime), "", theme::kBlue);
  snprintf(text, sizeof(text), "%04d-%02d-%02d  %02d:%02d", s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute);
  line(text, 80, theme::kBlue);
  if (s.clockError) alert(tr(Text::ClockError), 169, theme::kRed);
  else line(s.editing ? tr(Text::ClockKeys) : tr(Text::EnterCurrentTime), 157, theme::kMuted);
  const char* names[] = {tr(Text::Year), tr(Text::Month), tr(Text::Day), tr(Text::Hour), tr(Text::Minute), tr(Text::TimezoneMinutes)};
  int values[] = {s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute, s.offset};
  const char* modes[] = {tr(Text::ClockNormal), tr(Text::ClockTimezone), tr(Text::ClockCorrect)};
  for (unsigned i = 0; i < 9; ++i) {
    if (i < 6) {
      snprintf(text, sizeof(text), "%d", values[i]);
      item(s, i, names[i], menu_layout::kTop, 0, false, text);
    } else if (i == 6) {
      item(s, i, tr(Text::ClockMode), menu_layout::kTop, 0, false, modes[uint8_t(s.clockMode)]);
    } else if (i == 7) {
      item(s, i, tr(Text::SaveClock), menu_layout::kTop);
    } else {
      item(s, i, tr(Text::Back), menu_layout::kTop);
    }
  }
}
} }
