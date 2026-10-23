#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
void Screens::stamp(const Stamp& s, int top) {
  char text[100]; calendar::CivilTime local;
  if (s.timeKnown && calendar::fromUtc(s.utcSeconds, s.offsetMinutes, local)) {
    snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d", local.year, local.month, local.day, local.hour, local.minute);
    line(text, top, TFT_CYAN);
    const int offset = s.offsetMinutes < 0 ? -s.offsetMinutes : s.offsetMinutes;
    snprintf(text, sizeof(text), "UTC%c%02d:%02d · %s", s.offsetMinutes < 0 ? '-' : '+', offset / 60, offset % 60,
             s.dayComplete ? "完整日" : "部分日");
  } else {
    line("时间未知 · 尚未校准", top, TFT_YELLOW);
    text[0] = '\0';
  }
  line(text, top + 27);
  if (s.timeKnown) snprintf(text, sizeof(text), "今日 %llu 步", static_cast<unsigned long long>(s.dailySteps));
  else snprintf(text, sizeof(text), "当日步数未知");
  line(text, top + 55);
  snprintf(text, sizeof(text), "累计 %llu 步", static_cast<unsigned long long>(s.totalSteps)); line(text, top + 82);
}
void Screens::badges(uint8_t mask, int top) {
  char text[96];
  snprintf(text, sizeof(text), "每日：%s%s%s%s", mask & 1 ? "1千 " : "", mask & 2 ? "3千 " : "",
           mask & 4 ? "5千步" : "", mask & 7 ? "" : "尚无里程碑");
  line(text, top, TFT_YELLOW);
  snprintf(text, sizeof(text), "累计：%s%s%s%s", mask & 8 ? "1万 " : "", mask & 16 ? "5万 " : "",
           mask & 32 ? "10万步" : "", mask & 56 ? "" : "尚无里程碑");
  line(text, top + 26, TFT_YELLOW);
}
void Screens::records(const ScreenState& s, const Engine& e) {
  const auto& v = e.view();
  unsigned count = menuCount(s, e);
  char text[100];
  line(s.page == Page::FirstCaptures ? "永久首捕卡" : s.page == Page::Recent ? "最近捕捉" :
       s.page == Page::Days ? "每日步数" : "里程碑", 48, TFT_WHITE, true);
  if (s.selected + 1 == count) { line("返回记录目录", 200, TFT_CYAN); }
  else if (s.page == Page::FirstCaptures || s.page == Page::Recent) {
    const Capture& c = s.page == Page::FirstCaptures ? v.firstCapture[s.selected] : v.recent[v.recentSize - 1 - s.selected];
    uint16_t species = s.page == Page::FirstCaptures ? s.selected + 1 : c.species;
    snprintf(text, sizeof(text), "#%03u %s", species, speciesName(species)); line(text, 96, TFT_CYAN, true);
    if (!c.species) {
      line(v.starter == species ? "初始赠送 · 尚无实际首捕" : "还没有捕获记录", 180, TFT_YELLOW);
    } else {
      stamp(c.at, 150);
      snprintf(text, sizeof(text), "%s · 累计捕捉 %u 次", routeName(c.route), v.captureCounts[species - 1]); line(text, 270);
      badges(c.achievementMask, 304);
    }
  } else if (s.page == Page::Days) {
    const DayBucket& d = s.selected == 0 ? v.today : v.history[v.historySize - s.selected];
    if (d.dayKey) snprintf(text, sizeof(text), "%u · 计时段 %u", d.dayKey, d.epoch);
    else snprintf(text, sizeof(text), "日期未知");
    line(text, 112, TFT_CYAN);
    snprintf(text, sizeof(text), "%llu 步 · %s", static_cast<unsigned long long>(d.steps), d.complete ? "完整" : "部分记录"); line(text, 166);
    for (unsigned i = 0; i < 3; ++i) {
      snprintf(text, sizeof(text), "%u 步：%s", i == 0 ? 1000 : i == 1 ? 3000 : 5000,
               d.milestones[i].reached ? "已达成" : "未达成"); line(text, 220 + 35 * i);
    }
  } else {
    bool daily = s.selected < 3; unsigned i = s.selected % 3;
    const auto& a = daily ? v.dailyFirst[i] : v.lifetime[i];
    const unsigned thresholds[] = {1000, 3000, 5000, 10000, 50000, 100000};
    snprintf(text, sizeof(text), "%s %u 步", daily ? "每日" : "累计", thresholds[s.selected]); line(text, 112, TFT_CYAN);
    if (a.reached) stamp(a.first, 166); else line("尚未达成", 210, TFT_YELLOW);
  }
  snprintf(text, sizeof(text), "%u / %u · 轻触下方返回", s.selected + 1, count); line(text, 365, TFT_DARKGREY);
}
} }
