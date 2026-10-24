#include "screens.hpp"
#include "board.hpp"
#include "encounter_catalog.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
void Screens::menu(const ScreenState& s, const Engine& e, int top) {
  const auto phase = e.view().phase;
  const char* const home[] = {phase == Phase::Walking ? "暂无遭遇" : phase == Phase::ThrowReady ? "继续捕捉" : "继续遭遇", "选择路线", "我的伙伴", "151 图鉴", "捕捉与步行记录", "日期与时间", "捕获练习", "立即省电", "返回探索"};
  const char* const date[] = {"设置日期时间", "暂不设置", "浏览图鉴"};
  const char* const starters[] = {"妙蛙种子", "小火龙", "杰尼龟", "返回"};
  const char* const routes[] = {"森林", "海边", "洞窟", "返回"};
  const char* const encounter[] = {"开始对战", "捕获练习", "选择伙伴", "稍后再战"};
  const char* const pause[] = {"继续", "确认撤退"};
  const char* const records[] = {"永久首捕 · 151种", "最近32次捕捉", "今日与14日日汇总", "每日与长期里程碑", "返回"};
  const char* const error[] = {"重试保存 / 读取", "浏览图鉴"};
  const char* const* labels = nullptr;
  switch (s.page) {
    case Page::Menu: labels = home; break;
    case Page::DateWelcome: labels = date; break;
    case Page::Starter: {
      const uint16_t ids[] = {1, 4, 7, 0};
      for (unsigned i = 0; i < 4; ++i) item(s, i, starters[i], top, ids[i]);
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
      item(s, row, "返回", top); return;
    }
    case Page::Result: item(s, 0, "确认结果 · 继续探索", top); return;
    default: return;
  }
  for (unsigned i = 0; i < menuCount(s, e); ++i) item(s, i, labels[i], top);
}
void Screens::home(const ScreenState& s, const Engine& e, uint32_t now) {
  const auto& v = e.view();
  char text[100];
  switch (s.page) {
    case Page::Menu:
      line("步行探险", 38, TFT_WHITE, true);
      snprintf(text, sizeof(text), "%s %llu 步", v.clock.trusted ? "今日" : "上次记录",
               static_cast<unsigned long long>(v.today.steps));
      line(text, 70, TFT_CYAN);
      ballCount(availableBalls(v), 100);
      snprintf(text, sizeof(text), "%s · %s", routeName(v.route), speciesName(v.partner)); line(text, 128);
      {
        const EvolutionView progress = e.evolutionView(v.partner);
        if (!progress.count) snprintf(text, sizeof(text), "陪伴 %llu 步 · 最终形态", static_cast<unsigned long long>(progress.walked));
        else snprintf(text, sizeof(text), "培养 %llu / %lu 步", static_cast<unsigned long long>(progress.available),
                      static_cast<unsigned long>(progress.options[0].cost));
        line(text, 156);
      }
      if (e.stepsIncomplete()) line("部分步数缺失", 184, TFT_RED);
      else if (e.loadResult().degraded) line("已从备用存档恢复", 184, TFT_YELLOW);
      else if (v.phase == Phase::Walking) {
        if (!remainingEncounters(v, v.route)) {
          const bool all = !remainingEncounters(v, Route::Forest) && !remainingEncounters(v, Route::Coast) &&
                           !remainingEncounters(v, Route::Cave);
          line(all ? "普通宝可梦已全部捕获" : "本路线已捕获完 · 可换路线", 184, TFT_YELLOW);
        } else {
          snprintf(text, sizeof(text), "下次遭遇还差 %llu 步", static_cast<unsigned long long>(
            v.explorationCredit < kEncounterSteps ? kEncounterSteps - v.explorationCredit : 0));
          line(text, 184, TFT_YELLOW);
        }
      } else line("已有遭遇 · 随时继续", 184, TFT_YELLOW);
      break;
    case Page::DateWelcome:
      line("先确认日期", 62, TFT_WHITE, true);
      line(v.clock.pending.active ? "上次校时未完成，请重新确认" : "设备离线计时，无需手机", 120, TFT_YELLOW);
      line("暂不设置仍可累计步数", 155);
      line("捕捉时间会记为未知", 182); break;
    case Page::Starter:
      line("选择最初的伙伴", 66, TFT_WHITE, true);
      line("只能领取一次", 112, TFT_CYAN);
      if (s.selected < 3) { const uint16_t ids[] = {1, 4, 7}; thumbnail(ids[s.selected], 233, 165, 2); }
      break;
    case Page::Routes:
      line("选择路线", 66, TFT_WHITE, true);
      line("只影响下一场遭遇", 145, TFT_CYAN); break;
    case Page::Partners:
      line("我的伙伴", 66, TFT_WHITE, true);
      {
        unsigned row = 0;
        for (uint16_t id = 1; id <= kSpeciesCount; ++id)
          if (owns(v, id) && row++ == s.selected) { thumbnail(id, 233, 150, 2); break; }
      }
      break;
    case Page::Encounter:
      line("已发现宝可梦", 55, TFT_WHITE, true);
      line(speciesName(v.encounter.species), 105, TFT_CYAN, true);
      line("左键躲避 · 右键攻击", 145); line("等待标记进入中心再按下", 176); break;
    case Page::Pause:
      line("已暂停", 70, TFT_WHITE, true);
      line(s.practice ? "练习不消耗机会" : "继续会重新开始下一拍", 145, TFT_CYAN); break;
    case Page::Result:
      line(v.result == Result::Captured ? "捕捉成功" : v.result == Result::Lost ? "对战结束" :
           v.result == Result::Retreated ? "已经撤退" : v.result == Result::Released ?
           (v.captureCounts[v.encounter.species - 1] ? "已捕获 · 遭遇结束" : "已放弃捕捉") :
           v.result == Result::MigrationRetired ? "旧遭遇已结清" : "宝可梦离开了", 52, TFT_WHITE, true);
      line(speciesName(v.encounter.species), 90, TFT_CYAN);
      if (v.result == Result::Captured && v.recentSize) {
        const auto& c = v.recent[v.recentSize - 1];
        const bool first = v.captureCounts[v.encounter.species - 1] == 1;
        if (first) {
          const auto& sp = data::kSprites[v.encounter.species - 1];
          const int radius = std::max(sp.width, sp.height) + 12;
          for (unsigned k = 0; k < 12; ++k) {
            const float a = 6.2831853f * (float(now % 8000) / 8000.0f) + float(k) * 0.5235988f;
            M5.Display.fillCircle(233 + int(radius * std::cos(a)), 172 + int(radius * std::sin(a)), 2, kGold);
          }
        }
        sprite(v.encounter.species, now, resultPlayback_, 172);
        if (first) line("✦ 首次捕获 ✦", 238, kGold);
        if (c.at.timeKnown) {
          calendar::CivilTime local;
          if (calendar::fromUtc(c.at.utcSeconds, c.at.offsetMinutes, local)) {
            snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d", local.year, local.month, local.day, local.hour, local.minute);
            line(text, 262, TFT_DARKGREY);
          }
        }
        line(routeName(c.route), 290, TFT_CYAN);
        badges(c.achievementMask, 312);
      } else if (v.result == Result::MigrationRetired) {
        line("升级已退回 500 探索步", 145, TFT_YELLOW);
        line("升级后每 1000 步获得一球", 180);
      } else line("伙伴仍在，继续步行探索", 155);
      menu(s, e, v.result == Result::Captured ? 372 : 355); return;
    case Page::Records:
      line("捕捉与步行记录", 66, TFT_WHITE, true);
      snprintf(text, sizeof(text), "每日最高 %llu 步", static_cast<unsigned long long>(v.bestDaySteps));
      line(text, 145, TFT_CYAN); break;
    case Page::Error:
      line(e.savePending() ? "暂时无法保存" : "存储不可用", 60, TFT_RED, true);
      if (e.savePending()) { line("保存失败 · 结果尚未确认", 120); line("重试将保留同一次结果", 155); }
      else {
        const char* message = "存储读写失败";
        switch (e.loadResult().status) {
          case LoadStatus::NeedsProvisioning: message = "存储未初始化"; break;
          case LoadStatus::Corrupt: message = "存档损坏 · 保留原文件"; break;
          case LoadStatus::UnsupportedVersion: message = "存档版本较新 · 请升级"; break;
          default: break;
        }
        line(message, 125, TFT_YELLOW); line("图鉴仍可浏览", 166);
      }
      break;
    default: break;
  }
  menu(s, e);
}
} }
