#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
void Screens::clock(const ScreenState& s) {
  line(s.editing ? "左减 · 右加 · 双键完成" : "日期与时间", 48, TFT_WHITE, !s.editing);
  char text[90];
  snprintf(text, sizeof(text), "%04d-%02d-%02d  %02d:%02d", s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute);
  line(text, 102, TFT_CYAN);
  line(s.clockError ? "日期无效或校时失败，请重新确认" : "手动输入当前时间", 145, s.clockError ? TFT_RED : TFT_DARKGREY);
  const char* names[] = {"年", "月", "日", "时", "分", "时区分钟", "校时模式", "保存并校时", "返回"};
  int values[] = {s.civil.year, s.civil.month, s.civil.day, s.civil.hour, s.civil.minute, s.offset};
  const char* modes[] = {"普通校时", "时区变更", "修正错误日期"};
  for (unsigned i = 0; i < 9; ++i) {
    if (i < 6) snprintf(text, sizeof(text), "%s  %d%s", names[i], values[i], s.editing && i == s.selected ? "  ← →" : "");
    else if (i == 6) snprintf(text, sizeof(text), "%s", modes[uint8_t(s.clockMode)]);
    else snprintf(text, sizeof(text), "%s", names[i]);
    item(s, i, text);
  }
}
} }
