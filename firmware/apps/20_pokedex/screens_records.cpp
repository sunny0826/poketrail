#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
namespace {
void footer(const ScreenState& s, unsigned count) {
  char text[32]; snprintf(text, sizeof(text), "%u / %u", s.selected + 1, count);
  M5.Display.setFont(&fonts::efontCN_16); M5.Display.setTextColor(theme::kMuted);
  M5.Display.setTextDatum(middle_center); M5.Display.drawString(text, 233, 400);
  M5.Display.drawLine(163, 394, 157, 400, theme::kMuted);
  M5.Display.drawLine(157, 400, 163, 406, theme::kMuted);
  M5.Display.drawLine(303, 394, 309, 400, theme::kMuted);
  M5.Display.drawLine(309, 400, 303, 406, theme::kMuted);
  M5.Display.drawString("轻触下方返回", 233, 426);
}
void number(const char* text, int y) {
  M5.Display.setTextDatum(middle_center); M5.Display.setFont(&fonts::Font4);
  if (M5.Display.textWidth(text) > 320) M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(theme::kText); M5.Display.drawString(text, 233, y);
}
void star(int x, int y, int radius, uint16_t color) {
  M5.Display.drawLine(x - radius, y, x + radius, y, color);
  M5.Display.drawLine(x, y - radius, x, y + radius, color);
}
}
void Screens::stamp(const Stamp& s, int top) {
  char text[110]; calendar::CivilTime local;
  if (s.timeKnown && calendar::fromUtc(s.utcSeconds, s.offsetMinutes, local)) {
    snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d", local.year, local.month, local.day, local.hour, local.minute);
    line(text, top, theme::kGold);
    const int offset = s.offsetMinutes < 0 ? -s.offsetMinutes : s.offsetMinutes;
    snprintf(text, sizeof(text), "UTC%c%02d:%02d · %s", s.offsetMinutes < 0 ? '-' : '+', offset / 60, offset % 60,
             s.dayComplete ? "完整日" : "部分日");
    line(text, top + 25, theme::kMuted);
  } else {
    line("时间未知 · 尚未校准", top, theme::kGold);
    line("当日步数未知", top + 25, theme::kMuted);
  }
  if (s.timeKnown) {
    snprintf(text, sizeof(text), "今日 %llu 步", static_cast<unsigned long long>(s.dailySteps));
    line(text, top + 48, theme::kText);
  }
  snprintf(text, sizeof(text), "累计 %llu 步", static_cast<unsigned long long>(s.totalSteps));
  line(text, top + 70, theme::kText);
}
void Screens::badges(uint8_t mask, int top) {
  char text[96];
  snprintf(text, sizeof(text), "每日：%s%s%s%s", mask & 1 ? "1千 " : "", mask & 2 ? "3千 " : "",
           mask & 4 ? "5千步" : "", mask & 7 ? "" : "尚无里程碑");
  line(text, top, theme::kGold);
  snprintf(text, sizeof(text), "累计：%s%s%s%s", mask & 8 ? "1万 " : "", mask & 16 ? "5万 " : "",
           mask & 32 ? "10万步" : "", mask & 56 ? "" : "尚无里程碑");
  line(text, top + 26, theme::kGold);
}
void Screens::records(const ScreenState& s, const Engine& e) {
  const auto& v = e.view();
  const unsigned count = menuCount(s, e);
  char text[110];
  title(s.page == Page::FirstCaptures ? "永久首捕卡" : s.page == Page::Recent ? "最近捕捉" :
        s.page == Page::Days ? "每日步数" : "里程碑", "", theme::kGold);
  if (s.selected + 1 >= count) {
    line("返回记录目录", 200, theme::kMint);
    footer(s, count);
    return;
  }
  if (s.page == Page::FirstCaptures || s.page == Page::Recent) {
    const Capture& c = s.page == Page::FirstCaptures ? v.firstCapture[s.selected] : v.recent[v.recentSize - 1 - s.selected];
    const uint16_t species = s.page == Page::FirstCaptures ? s.selected + 1 : c.species;
    snprintf(text, sizeof(text), "#%03u %s", species, speciesName(species)); line(text, 80, theme::kGold);
  } else if (s.page == Page::Days) {
    const DayBucket& d = s.selected == 0 ? v.today : v.history[v.historySize - s.selected];
    if (d.dayKey) snprintf(text, sizeof(text), "%u · 计时段 %u", d.dayKey, d.epoch);
    else snprintf(text, sizeof(text), "日期未知");
    line(text, 80, theme::kBlue);
  } else if (s.page == Page::Achievements) {
    const bool daily = s.selected < 3;
    const unsigned thresholds[] = {1000, 3000, 5000, 10000, 50000, 100000};
    snprintf(text, sizeof(text), "%s %u 步", daily ? "每日" : "累计", thresholds[s.selected]); line(text, 80, theme::kGold);
  }
  if (s.page == Page::FirstCaptures || s.page == Page::Recent) {
    const Capture& c = s.page == Page::FirstCaptures ? v.firstCapture[s.selected] : v.recent[v.recentSize - 1 - s.selected];
    const uint16_t species = s.page == Page::FirstCaptures ? s.selected + 1 : c.species;
    if (!c.species) {
      thumbnail(species, 233, 162, 2);
      alert(v.starter == species ? "初始赠送 · 尚无实际首捕" : "还没有捕获记录", 230, theme::kGold);
      line("自由浏览不会产生首捕卡", 271, theme::kMuted);
    } else {
      actorShadow(233, 157);
      sprite(species, 0, resultPlayback_, 157, 233, 0, 2, 120);
      snprintf(text, sizeof(text), "%s · 累计捕捉 %u 次", routeName(c.route), v.captureCounts[species - 1]);
      line(text, 229, theme::kText);
      stamp(c.at, 258);
      badges(c.achievementMask, 350);
    }
  } else if (s.page == Page::Days) {
    const DayBucket& d = s.selected == 0 ? v.today : v.history[v.historySize - s.selected];
    snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(d.steps)); number(text, 141);
    line(d.complete ? "步 · 完整日" : "步 · 部分记录", 181, theme::kMuted);
    for (unsigned i = 0; i < 3; ++i) {
      const int y = 235 + i * 48;
      const bool reached = d.milestones[i].reached;
      const uint16_t color = reached ? theme::kMint : theme::kDisabled;
      M5.Display.drawCircle(136, y, 10, color);
      if (reached) {
        M5.Display.drawLine(131, y, 136, y + 5, color);
        M5.Display.drawLine(136, y + 5, 142, y - 5, color);
      }
      char label[24]; snprintf(label, sizeof(label), "%u", i == 0 ? 1000 : i == 1 ? 3000 : 5000);
      M5.Display.setTextDatum(middle_left); M5.Display.setFont(&fonts::efontCN_16);
      M5.Display.setTextColor(reached ? theme::kText : theme::kMuted); M5.Display.drawString(label, 161, y);
      M5.Display.setTextDatum(middle_right); M5.Display.setTextColor(color);
      M5.Display.drawString(reached ? "已达成" : "未达成", 337, y);
    }
  } else if (s.page == Page::Achievements) {
    const bool daily = s.selected < 3;
    const unsigned i = s.selected % 3;
    const auto& a = daily ? v.dailyFirst[i] : v.lifetime[i];
    const uint16_t color = a.reached ? theme::kGold : theme::kDisabled;
    M5.Display.drawCircle(233, 156, 50, color);
    star(233, 156, 22, color);
    M5.Display.fillTriangle(202, 191, 216, 187, 211, 224, color);
    M5.Display.fillTriangle(250, 187, 264, 191, 255, 224, color);
    if (a.reached) stamp(a.first, 253);
    else {
      line("尚未达成", 254, theme::kGold, true);
      const unsigned thresholds[] = {1000, 3000, 5000, 10000, 50000, 100000};
      const uint64_t current = daily ? v.today.steps : v.totalSteps;
      progress(current, thresholds[s.selected], 298, theme::kGold);
      snprintf(text, sizeof(text), "%llu / %u 步", static_cast<unsigned long long>(current), thresholds[s.selected]);
      line(text, 326, theme::kText);
    }
  }
  footer(s, count);
}
} }
