#include "screens.hpp"
#include "board.hpp"
#include "encounter_catalog.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace pokedex { namespace ui {
using namespace adventure;
namespace {
theme::Icon iconFor(Page page, unsigned index) {
  using I = theme::Icon;
  switch (page) {
    case Page::Menu: {
      const I icons[] = {I::Practice,I::Route,I::Partner,I::Book,I::Records,I::Clock,I::Practice,I::Sleep,I::Home};
      return icons[index];
    }
    case Page::DateWelcome: { const I icons[] = {I::Clock,I::Next,I::Book}; return icons[index]; }
    case Page::Routes: return index < 3 ? I::Route : I::Back;
    case Page::Encounter: { const I icons[] = {I::Practice,I::Practice,I::Partner,I::Back}; return icons[index]; }
    case Page::Pause: return index ? I::Back : I::Next;
    case Page::Records: return index == 0 ? I::Book : index == 4 ? I::Back : I::Records;
    case Page::Error: return index ? I::Book : I::Next;
    default: return I::Dot;
  }
}
void bookIcon() {
  M5.Display.drawRect(207, 124, 52, 44, theme::kGold);
  M5.Display.drawLine(233, 124, 233, 168, theme::kGold);
  for (int y : {135, 145, 155}) {
    M5.Display.drawLine(214, y, 226, y, theme::kGold);
    M5.Display.drawLine(240, y, 252, y, theme::kGold);
  }
}
}
void Screens::menu(const ScreenState& s, const Engine& e, int top) {
  const auto phase = e.view().phase;
  const char* const home[] = {phase == Phase::Walking ? "暂无遭遇" : phase == Phase::ThrowReady ? "继续捕捉" : "继续遭遇", "选择路线", "我的伙伴", "151 图鉴", "捕捉与步行记录", "日期与时间", "免费练习", "立即省电", "返回探索"};
  const char* const date[] = {"设置日期时间", "暂不设置", "浏览图鉴"};
  const char* const starters[] = {"妙蛙种子", "小火龙", "杰尼龟", "返回"};
  const char* const routes[] = {"森林", "海边", "洞窟", "返回"};
  const char* const encounter[] = {"开始对战", "免费练习", "选择伙伴", "稍后再战"};
  const char* const pause[] = {"继续", "确认撤退"};
  const char* const records[] = {"永久首捕 · 151种", "最近32次捕捉", "今日与14日日汇总", "每日与长期里程碑", "返回"};
  const char* const error[] = {"重试保存 / 读取", "浏览图鉴"};
  const char* const* labels = nullptr;
  switch (s.page) {
    case Page::Menu: labels = home; break;
    case Page::DateWelcome: labels = date; break;
    case Page::Starter: {
      const uint16_t ids[] = {1, 4, 7, 0};
      for (unsigned i = 0; i < 4; ++i) item(s, i, starters[i], top, ids[i], false,
        i == 3 ? theme::Icon::Back : theme::Icon::Partner);
      return;
    }
    case Page::Routes: labels = routes; break;
    case Page::Encounter: labels = encounter; break;
    case Page::Pause: labels = pause; break;
    case Page::Records: labels = records; break;
    case Page::Error: labels = error; break;
    case Page::Partners: {
      unsigned row = 0;
      for (unsigned id = 1; id <= 151; ++id) if (owns(e.view(), id)) {
        item(s, row++, speciesName(id), top, id, id == e.view().partner);
      }
      item(s, row, "返回", top, 0, false, theme::Icon::Back); return;
    }
    case Page::Result: item(s, 0, "确认结果 · 继续探索", top, 0, false, theme::Icon::Next); return;
    default: return;
  }
  for (unsigned i = 0; i < menuCount(s, e); ++i)
    item(s, i, labels[i], top, 0, false, iconFor(s.page, i));
}
void Screens::home(const ScreenState& s, const Engine& e, uint32_t now) {
  const auto& v = e.view();
  char text[100];
  switch (s.page) {
    case Page::Menu: {
      title("步行探险", "", theme::kMint);
      snprintf(text, sizeof(text), "%s %llu 步 · %s", v.clock.trusted ? "今日" : "上次记录",
               static_cast<unsigned long long>(v.today.steps), routeName(v.route));
      line(text, 80, theme::kMint);
      ballCount(availableBalls(v), 110, 327);
      const EvolutionView progress = e.evolutionView(v.partner);
      snprintf(text, sizeof(text), "%s · %s", speciesName(v.partner), progress.count ? "培养" : "最终形态");
      line(text, 132, theme::kMuted);
      if (!progress.count) snprintf(text, sizeof(text), "陪伴 %llu 步", static_cast<unsigned long long>(progress.walked));
      else snprintf(text, sizeof(text), "%llu / %lu 步", static_cast<unsigned long long>(progress.available),
                    static_cast<unsigned long>(progress.options[0].cost));
      line(text, 156, theme::kText);
      if (e.stepsIncomplete()) alert("部分步数缺失", 183, theme::kRed);
      else if (e.loadResult().degraded) alert("已从备用存档恢复", 183, theme::kGold);
      else if (v.phase == Phase::Walking) {
        if (!remainingEncounters(v, v.route)) {
          const bool all = !remainingEncounters(v, Route::Forest) && !remainingEncounters(v, Route::Coast) &&
                           !remainingEncounters(v, Route::Cave);
          alert(all ? "普通宝可梦已全部捕获" : "本路线已捕获完 · 可换路线", 183, theme::kGold);
        } else {
          snprintf(text, sizeof(text), "下次遭遇还差 %llu 步", static_cast<unsigned long long>(
            v.explorationCredit < kEncounterSteps ? kEncounterSteps - v.explorationCredit : 0));
          alert(text, 183, theme::kGold);
        }
      } else alert("已有遭遇 · 随时继续", 183, theme::kGold);
      break;
    }
    case Page::DateWelcome:
      title("先确认日期", v.clock.pending.active ? "上次校时未完成，请重新确认" : "设备离线计时，无需手机", theme::kGold);
      M5.Display.drawCircle(233, 133, 29, theme::kGold);
      M5.Display.drawLine(233, 111, 233, 133, theme::kGold);
      M5.Display.drawLine(233, 133, 249, 142, theme::kGold);
      line("暂不设置仍可累计步数", 174, theme::kText);
      line("捕捉时间会记为未知", 194, theme::kMuted);
      break;
    case Page::Starter:
      title("选择最初的伙伴", "只能领取一次", theme::kGold);
      if (s.selected < 3) {
        const uint16_t ids[] = {1, 4, 7};
        actorShadow(233, 151);
        sprite(ids[s.selected], now, resultPlayback_, 151, 233, 0, 2);
      }
      break;
    case Page::Routes:
      title("选择路线", "只影响下一场遭遇", theme::kMint);
      sceneThumbnail(s.selected < 3 ? Route(s.selected) : Route::Forest, 233, 151);
      break;
    case Page::Partners: {
      title("我的伙伴", "", theme::kMint);
      unsigned owned = 0, row = 0;
      for (uint16_t id = 1; id <= kSpeciesCount; ++id) if (owns(v, id)) ++owned;
      snprintf(text, sizeof(text), "%u 位已拥有", owned); line(text, 80, theme::kMint);
      for (uint16_t id = 1; id <= kSpeciesCount; ++id) if (owns(v, id) && row++ == s.selected) {
        M5.Display.drawCircle(233, 136, 48, theme::kOutline);
        thumbnail(id, 233, 137, 2); break;
      }
      break;
    }
    case Page::Encounter:
      scene(v.encounter.route, true);
      title("发现宝可梦", speciesName(v.encounter.species), theme::kGold);
      actorShadow(233, 147);
      sprite(v.encounter.species, now, resultPlayback_, 147, 233, 0, 2, 110);
      break;
    case Page::Pause:
      title("已暂停", s.practice ? "练习不消耗机会" : "继续后进入下一拍", theme::kGold);
      M5.Display.fillRect(216, 120, 13, 44, theme::kGold);
      M5.Display.fillRect(239, 120, 13, 44, theme::kGold);
      break;
    case Page::Result:
      title(v.result == Result::Captured ? "捕捉成功" : v.result == Result::Lost ? "对战结束" :
            v.result == Result::Retreated ? "已经撤退" : v.result == Result::Released ?
            (v.captureCounts[v.encounter.species - 1] ? "已捕获 · 遭遇结束" : "已放弃捕捉") :
            v.result == Result::MigrationRetired ? "旧遭遇已结清" : "宝可梦离开了",
            speciesName(v.encounter.species),
            v.result == Result::Captured || v.result == Result::MigrationRetired ? theme::kGold : theme::kMuted);
      if (v.result == Result::Captured && v.recentSize) {
        const auto& c = v.recent[v.recentSize - 1];
        const bool first = v.captureCounts[v.encounter.species - 1] == 1;
        if (first) {
          const auto& sp = data::kSprites[v.encounter.species - 1];
          const int scale = sp.height * 2 <= 120 ? 2 : 1;
          const int radiusX = sp.width * scale / 2 + 12;
          const int radiusY = std::min(64, sp.height * scale / 2 + 12);
          for (unsigned k = 0; k < 12; ++k) {
            const float a = 6.2831853f * (float(now % 8000) / 8000.0f) + float(k) * 0.5235988f;
            M5.Display.fillCircle(233 + int(radiusX * std::cos(a)), 168 + int(radiusY * std::sin(a)), 2, theme::kGold);
          }
        }
        actorShadow(233, 168);
        sprite(v.encounter.species, now, resultPlayback_, 168, 233, 0, 2, 120);
        if (first) line("首次捕获", 246, theme::kGold);
        if (c.at.timeKnown) {
          calendar::CivilTime local;
          if (calendar::fromUtc(c.at.utcSeconds, c.at.offsetMinutes, local)) {
            snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d", local.year, local.month, local.day, local.hour, local.minute);
            line(text, 270, theme::kMuted);
          }
        } else line("捕捉时间未知", 270, theme::kMuted);
        line(routeName(c.route), 290, theme::kMint);
        badges(c.achievementMask, 312);
      } else {
        M5.Display.drawCircle(233, 164, 52, theme::kOutline);
        thumbnail(v.encounter.species, 233, 164, 2);
        if (v.result == Result::MigrationRetired) {
          line("升级已退回 500 探索步", 249, theme::kGold);
          line("升级后每 1000 步获得一球", 277, theme::kText);
        } else if (v.result == Result::Released && !v.captureCounts[v.encounter.species - 1]) {
          line("不退探索步数或已用精灵球", 249, theme::kRed);
        } else line("伙伴仍在，继续步行探索", 249, theme::kMuted);
      }
      menu(s, e, v.result == Result::Captured ? 372 : 355); return;
    case Page::Records:
      title("捕捉与步行记录", "", theme::kGold);
      snprintf(text, sizeof(text), "每日最高 %llu 步", static_cast<unsigned long long>(v.bestDaySteps));
      line(text, 80, theme::kGold); bookIcon();
      break;
    case Page::Error:
      title(e.savePending() ? "暂时无法保存" : "存储不可用", e.savePending() ? "结果尚未确认" : "玩法已暂停", theme::kRed);
      warning(233, 133, theme::kRed);
      if (e.savePending()) {
        line("保存失败 · 重试将保留同一次结果", 184, theme::kGold);
      } else {
        const char* message = "存储读写失败";
        switch (e.loadResult().status) {
          case LoadStatus::NeedsProvisioning: message = "存储未初始化"; break;
          case LoadStatus::Corrupt: message = "存档损坏 · 保留原文件"; break;
          case LoadStatus::UnsupportedVersion: message = "存档版本较新 · 请升级"; break;
          default: break;
        }
        line(message, 184, theme::kGold);
      }
      break;
    default: break;
  }
  menu(s, e);
}
} }
