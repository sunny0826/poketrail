#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
void Screens::clock(const ScreenState& s) {
  char text[90];
  title(s.editing ? "编辑日期与时间" : "日期与时间", "", theme::kBlue);
  snprintf(text, sizeof(text), "%04d-%02d-%02d  %02d:%02d", s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute);
  line(text, 80, theme::kBlue);
  if (s.clockError) alert("日期无效或校时失败，请重新确认", 169, theme::kRed);
  else line(s.editing ? "左减 · 右加 · 双键完成" : "手动输入当前时间", 157, theme::kMuted);
  const char* names[] = {"年", "月", "日", "时", "分", "时区分钟"};
  int values[] = {s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute, s.offset};
  const char* modes[] = {"普通校时", "时区变更", "修正错误日期"};
  for (unsigned i = 0; i < 9; ++i) {
    if (i < 6) {
      snprintf(text, sizeof(text), "%d", values[i]);
      item(s, i, names[i], menu_layout::kTop, 0, false, text);
    } else if (i == 6) {
      item(s, i, "校时模式", menu_layout::kTop, 0, false, modes[uint8_t(s.clockMode)]);
    } else if (i == 7) {
      item(s, i, "保存并校时", menu_layout::kTop);
    } else {
      item(s, i, "返回", menu_layout::kTop);
    }
  }
}
} }
