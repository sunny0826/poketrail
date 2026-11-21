#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
namespace {
void footer(const ScreenState& s, unsigned count) {
  char text[32]; snprintf(text, sizeof(text), "%u / %u", s.selected + 1, count);
  M5.Display.setFont(&fonts::efontCN_16); M5.Display.setTextColor(theme::kMuted);
  M5.Display.setTextDatum(middle_center); M5.Display.drawString(text, 233, 400);
  M5.Display.drawLine(163, 394, 157, 400, theme::kMuted);
  M5.Display.drawLine(157, 400, 163, 406, theme::kMuted);
  M5.Display.drawLine(303, 394, 309, 400, theme::kMuted);
  M5.Display.drawLine(309, 400, 303, 406, theme::kMuted);
}
bool duplicateDayKey(const Save& v, uint32_t dayKey) {
  if (!dayKey) return false;
  unsigned matches = v.today.dayKey == dayKey ? 1u : 0u;
  for (uint8_t i = 0; i < v.historySize; ++i)
    if (v.history[i].dayKey == dayKey) ++matches;
  return matches > 1;
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
             s.dayComplete ? tr(Text::FullDay) : tr(Text::PartialDay));
    line(text, top + 25, theme::kMuted);
  } else {
    line(tr(Text::TimeUnknown), top, theme::kGold);
  }
  if (s.timeKnown) {
    snprintf(text, sizeof(text), tr(Text::TodaySteps), static_cast<unsigned long long>(s.dailySteps));
    line(text, top + 48, theme::kText);
  }
  snprintf(text, sizeof(text), tr(Text::TotalSteps), static_cast<unsigned long long>(s.totalSteps));
  line(text, s.timeKnown ? top + 70 : top + 28, theme::kText);
}
void Screens::badges(uint8_t mask, int top) {
  char text[96];
  int y = top;
  if (mask & 7) {
    snprintf(text, sizeof(text), tr(Text::DailyBadges), mask & 1 ? tr(Text::OneThousand) : "",
             mask & 2 ? tr(Text::ThreeThousand) : "", mask & 4 ? tr(Text::FiveThousand) : "");
    line(text, y, theme::kGold);
    y += 26;
  }
  if (mask & 56) {
    snprintf(text, sizeof(text), tr(Text::TotalBadges), mask & 8 ? tr(Text::TenThousand) : "",
             mask & 16 ? tr(Text::FiftyThousand) : "", mask & 32 ? tr(Text::HundredThousand) : "");
    line(text, y, theme::kGold);
  }
}
void Screens::records(const ScreenState& s, const Engine& e) { drawRecords(s, e, true); }
void Screens::drawRecords(const ScreenState& s, const Engine& e, bool full) {
  const auto& v = e.view();
  const unsigned count = menuCount(s, e);
  uint8_t todayMask = 0, achMask = 0, selDayMask = 0;
  for (int i = 0; i < 3; ++i) {
    if (v.today.milestones[i].reached) todayMask |= (1u << i);
    if (v.dailyFirst[i].reached) achMask |= (1u << i);
    if (v.lifetime[i].reached) achMask |= (1u << (i + 3));
  }
  uint16_t selCap = 0;
  if ((s.page == Page::FirstCaptures || s.page == Page::Recent) && s.selected + 1 < count) {
    const Capture& c = s.page == Page::FirstCaptures ? v.firstCapture[s.selected] : v.recent[v.recentSize - 1 - s.selected];
    const uint16_t sp = s.page == Page::FirstCaptures ? s.selected + 1 : c.species;
    if (sp) selCap = v.captureCounts[sp - 1];
  }
  bool selDayComplete = false;
  uint64_t selDaySteps = 0;
  if (s.page == Page::Days && s.selected + 1 < count) {
    const DayBucket& d = s.selected == 0 ? v.today : v.history[v.historySize - s.selected];
    selDayComplete = d.complete; selDaySteps = d.steps;
    for (int i = 0; i < 3; ++i) if (d.milestones[i].reached) selDayMask |= (1u << i);
  }
  const bool dirty = full || s.selected != snap_.selected || v.rosterRevision != snap_.rosterRevision ||
                     v.recentSize != snap_.recentSize || v.historySize != snap_.historySize ||
                     v.today.steps != snap_.todaySteps || v.totalSteps != snap_.totalSteps ||
                     v.today.dayKey != snap_.todayDayKey || v.today.complete != snap_.todayComplete ||
                     todayMask != snap_.todayMilestonesMask || achMask != snap_.achievementsMask ||
                     selCap != snap_.selectedCaptureCount || v.starter != snap_.species ||
                     selDayComplete != snap_.selectedDayComplete ||
                     selDayMask != snap_.selectedDayMilestonesMask || selDaySteps != snap_.selectedDaySteps;
  if (full) {
    title(s.page == Page::FirstCaptures ? tr(Text::FirstCaptureCards) : s.page == Page::Recent ? tr(Text::RecentCaptures) :
          s.page == Page::Days ? tr(Text::DailySteps) : tr(Text::Milestones), "", theme::kGold);
  }
  if (dirty) {
    if (!full) {
      M5.Display.startWrite();
      clearRegion(40, 68, 388, 344, theme::kBackground);
    }
    char text[110];
    if (s.selected + 1 >= count) {
      line(tr(Text::BackToRecords), 200, theme::kMint);
      footer(s, count);
    } else {
      if (s.page == Page::FirstCaptures || s.page == Page::Recent) {
        const Capture& c = s.page == Page::FirstCaptures ? v.firstCapture[s.selected] : v.recent[v.recentSize - 1 - s.selected];
        const uint16_t species = s.page == Page::FirstCaptures ? s.selected + 1 : c.species;
        snprintf(text, sizeof(text), "#%03u %s", species, speciesName(species)); line(text, 80, theme::kGold);
        if (!c.species) {
          thumbnail(species, 233, 162, 2);
          alert(v.starter == species ? tr(Text::GiftNotCaught) : tr(Text::NoCaptureRecord), 230, theme::kGold);
        } else {
          actorShadow(233, 157);
          sprite(species, 0, resultPlayback_, 157, 233, 0, 2, 120);
          snprintf(text, sizeof(text), tr(Text::RouteCaptureCount), routeName(c.route), v.captureCounts[species - 1]);
          line(text, 229, theme::kText);
          stamp(c.at, 258);
          badges(c.achievementMask, 350);
        }
      } else if (s.page == Page::Days) {
        const DayBucket& d = s.selected == 0 ? v.today : v.history[v.historySize - s.selected];
        if (!d.dayKey) snprintf(text, sizeof(text), "%s", tr(Text::DateUnknown));
        else if (duplicateDayKey(v, d.dayKey)) snprintf(text, sizeof(text), tr(Text::DayEpoch), d.dayKey, d.epoch);
        else snprintf(text, sizeof(text), "%u", d.dayKey);
        line(text, 80, theme::kBlue);
        snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(d.steps)); number(text, 141);
        line(d.complete ? tr(Text::FullDaySteps) : tr(Text::PartialDaySteps), 181, theme::kMuted);
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
          M5.Display.drawString(reached ? tr(Text::Reached) : tr(Text::NotReached), 337, y);
        }
      } else if (s.page == Page::Achievements) {
        const bool daily = s.selected < 3;
        const unsigned thresholds[] = {1000, 3000, 5000, 10000, 50000, 100000};
        snprintf(text, sizeof(text), tr(Text::MilestoneSteps), daily ? tr(Text::Daily) : tr(Text::Total), thresholds[s.selected]); line(text, 80, theme::kGold);
        const unsigned i = s.selected % 3;
        const auto& a = daily ? v.dailyFirst[i] : v.lifetime[i];
        const uint16_t color = a.reached ? theme::kGold : theme::kDisabled;
        M5.Display.drawCircle(233, 156, 50, color);
        star(233, 156, 22, color);
        M5.Display.fillTriangle(202, 191, 216, 187, 211, 224, color);
        M5.Display.fillTriangle(250, 187, 264, 191, 255, 224, color);
        if (a.reached) stamp(a.first, 253);
        else {
          line(tr(Text::NotReachedYet), 254, theme::kGold, true);
          const uint64_t current = daily ? v.today.steps : v.totalSteps;
          progress(current, thresholds[s.selected], 298, theme::kGold);
          snprintf(text, sizeof(text), tr(Text::StepRatio), static_cast<unsigned long long>(current), thresholds[s.selected]);
          line(text, 326, theme::kText);
        }
      }
      footer(s, count);
    }
    if (!full) M5.Display.endWrite();
  }
  snap_.rosterRevision = v.rosterRevision;
  snap_.recentSize = v.recentSize;
  snap_.historySize = v.historySize;
  snap_.todaySteps = v.today.steps;
  snap_.totalSteps = v.totalSteps;
  snap_.todayDayKey = v.today.dayKey;
  snap_.todayComplete = v.today.complete;
  snap_.todayMilestonesMask = todayMask;
  snap_.achievementsMask = achMask;
  snap_.selectedCaptureCount = selCap;
  snap_.species = v.starter;
  snap_.selectedDayComplete = selDayComplete;
  snap_.selectedDayMilestonesMask = selDayMask;
  snap_.selectedDaySteps = selDaySteps;
}
} }
