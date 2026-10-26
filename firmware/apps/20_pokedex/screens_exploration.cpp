#include "screens.hpp"
#include "board.hpp"
#include "sprites_generated.hpp"
#include <cstdio>
#include <cstring>

namespace pokedex { namespace ui {
using namespace adventure;
namespace {
void formatSteps(uint64_t value, char* output, size_t size) {
  char digits[24]; snprintf(digits, sizeof(digits), "%llu", static_cast<unsigned long long>(value));
  const int length = strlen(digits);
  int groups = (length - 1) / 3;
  int out = 0;
  for (int index = 0; index < length && out + 1 < int(size); ++index) {
    output[out++] = digits[index];
    if (groups && index == length - groups * 3 - 1) { output[out++] = ','; --groups; }
  }
  output[out] = '\0';
}
void smallLabel(const char* text, int x, int y, uint16_t color, lgfx::textdatum_t datum) {
  M5.Display.setTextDatum(datum);
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextColor(color);
  M5.Display.drawString(text, x, y);
}
void todayNumber(const char* text, bool partial) {
  M5.Display.setFont(&fonts::Font4);
  const int width = M5.Display.textWidth(text);
  if (width > 180) {
    smallLabel(partial ? "今日已记录 · 步" : "今日 · 步", 233, 380, theme::kMuted, middle_center);
    const lgfx::IFont* numberFont = width > 280 ? static_cast<const lgfx::IFont*>(&fonts::Font0)
                                                : static_cast<const lgfx::IFont*>(&fonts::Font2);
    M5.Display.setFont(numberFont);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(theme::kText);
    M5.Display.drawString(text, 233, 407);
    return;
  }
  smallLabel(partial ? "今日已记录" : "今日", partial ? 139 : 158, 396, theme::kMuted, middle_center);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(theme::kText);
  M5.Display.drawString(text, partial ? 250 : 237, 393);
  smallLabel("步", partial ? 329 : 316, 397, theme::kMuted, middle_center);
}
const char* patternName(TelegraphPattern pattern) {
  switch (pattern) {
    case TelegraphPattern::AlternateDodgeFirst: return "闪避先手";
    case TelegraphPattern::AttackOnly: return "连续攻击";
    case TelegraphPattern::DodgeOnly: return "连续闪避";
    case TelegraphPattern::Alternate:
    default: return "交替";
  }
}
const char* clueTask(uint8_t index) {
  switch (index) {
    case 1: return "至少两次完美";
    case 2: return "无伤胜利";
    default: return "胜利即可";
  }
}
uint8_t clueCount(const Save& save) {
  uint8_t count = 0;
  for (uint8_t index = 0; index < kRuinsClueTotal; ++index)
    if (save.ruins.clueMask & ruinsClueBit(index)) ++count;
  return count;
}
void specLine(const BattleSpec& spec, int y) {
  char text[64];
  snprintf(text, sizeof(text), "生命 我方%u / 首领%u", unsigned(spec.playerHp), unsigned(spec.enemyHp));
  smallLabel(text, 233, y - 12, theme::kText, middle_center);
  snprintf(text, sizeof(text), "节拍 %.1f 秒 · %u 拍", double(spec.beatMs) / 1000.0, unsigned(spec.cueLimit));
  smallLabel(text, 233, y + 12, theme::kMuted, middle_center);
  snprintf(text, sizeof(text), "节奏 %s · 判定 %u/%ums", patternName(spec.telegraph),
           unsigned(spec.normalWindowMs), unsigned(spec.perfectWindowMs));
  smallLabel(text, 233, y + 36, theme::kMuted, middle_center);
}
}

void Screens::ruins(const ScreenState& s, const Engine& e, uint32_t now) {
  (void)now;
  const auto& save = e.view();
  const RuinsState& ruins = save.ruins;
  char text[72];
  switch (s.page) {
    case Page::Ruins: {
      const uint8_t found = clueCount(save);
      title("传说遗迹", ruins.unlocked ? "首领挑战永久开放" :
            found == kRuinsClueTotal ? "线索已齐 · 排列路线" : "收集三条线索", theme::kPurple);
      // One chip per clue: the route name and its state, never colour alone.
      for (uint8_t index = 0; index < kRuinsClueTotal; ++index) {
        const RuinsClueSpec clue = ruinsClueAt(index);
        const bool got = ruins.clueMask & ruinsClueBit(index);
        const int cx = 108 + int(index) * 125;
        const uint16_t color = got ? theme::routeColor(clue.route) : theme::kMuted;
        M5.Display.fillRoundRect(cx - 57, 104, 114, 52, 12, got ? theme::kPanelActive : theme::kPanel);
        M5.Display.drawRoundRect(cx - 57, 104, 114, 52, 12, got ? color : theme::kOutline);
        smallLabel(routeName(clue.route), cx, 119, color, middle_center);
        smallLabel(got ? "已获得" : "未获得", cx, 141, got ? theme::kMint : theme::kMuted, middle_center);
      }
      if (ruins.unlocked) {
        const uint8_t target = ruinsTargetIndex(ruins.lastTarget);
        if (target == kRuinsNoIndex) {
          line("请选择目标", 190, theme::kGold);
        } else {
          const RuinsTargetSpec spec = ruinsTargetAt(target);
          // Both summary rows stay above the menu panel (top edge y=208) and
          // below the clue chips (bottom edge y=156).
          snprintf(text, sizeof(text), "目标 %s", speciesName(spec.species));
          line(text, 172, theme::kText);
          snprintf(text, sizeof(text), "生命 %u · 节拍 %.2f 秒 · %s", unsigned(spec.battle.enemyHp),
                   double(spec.battle.beatMs) / 1000.0, patternName(spec.battle.telegraph));
          line(text, 196, theme::kMuted);
        }
      } else {
        snprintf(text, sizeof(text), "线索 %u / %u", unsigned(found), unsigned(kRuinsClueTotal));
        line(text, 196, found == kRuinsClueTotal ? theme::kGold : theme::kText);
      }
      break;
    }
    case Page::RuinsClues: {
      title("寻找线索", "胜利只给线索 · 不进入投球", theme::kPurple);
      uint8_t index = s.selected < kRuinsClueTotal ? uint8_t(s.selected) : 0;
      const RuinsClueSpec clue = ruinsClueAt(index);
      sceneThumbnail(clue.route, 233, 136);
      if (s.command == CommandResult::InvalidPhase) line("先完成当前遭遇", 196, theme::kGold);
      else {
        // An earned clue also names the route-order slot it has to fill.
        if (ruins.clueMask & ruinsClueBit(index))
          snprintf(text, sizeof(text), "第%u位 · %s · %s", unsigned(ruinsClueOrderPosition(index)),
                   routeName(clue.route), clueTask(index));
        else snprintf(text, sizeof(text), "%s · %s", routeName(clue.route), clueTask(index));
        line(text, 196, ruins.clueMask & ruinsClueBit(index) ? theme::kMint : theme::kGold);
      }
      break;
    }
    case Page::RuinsOrder: {
      title("排列路线", "按线索标出的顺序", theme::kPurple);
      for (uint8_t slot = 0; slot < kRuinsClueTotal; ++slot) {
        const int cx = 130 + int(slot) * 103;
        const bool filled = slot < s.orderCount;
        const bool conflict = filled && s.orderConflict == slot;
        const uint16_t color = conflict ? theme::kRed :
                               filled ? theme::routeColor(Route(s.orderInput[slot])) : theme::kOutline;
        M5.Display.fillCircle(cx, 132, 34, filled ? theme::kPanelActive : theme::kPanel);
        M5.Display.drawCircle(cx, 132, 34, color);
        smallLabel(filled ? routeName(Route(s.orderInput[slot])) : "?", cx, 132, color, middle_center);
      }
      if (s.orderConflict != kRuinsNoIndex) {
        snprintf(text, sizeof(text), "第 %u 位冲突", unsigned(s.orderConflict) + 1);
        line(text, 188, theme::kRed);
      } else if (ruins.clueMask != kRuinsAllClues) {
        line("线索未齐", 188, theme::kGold);
      } else if (s.command == CommandResult::InvalidPhase) {
        line("先完成当前遭遇", 188, theme::kGold);
      } else {
        snprintf(text, sizeof(text), "选择第 %u 位", unsigned(s.orderCount) + 1);
        line(text, 188, theme::kText);
      }
      break;
    }
    case Page::RuinsTargets: {
      snprintf(text, sizeof(text), "已捕获 %u / %u", unsigned(ruinsCapturedTargets(save)),
               unsigned(kRuinsTargetTotal));
      title("选择目标", text, theme::kPurple);
      if (s.command == CommandResult::InvalidPhase) alert("已有遭遇", 196, theme::kGold);
      break;
    }
    case Page::RuinsBoss: {
      const uint8_t index = ruinsTargetIndex(ruins.lastTarget);
      if (index == kRuinsNoIndex) { title("传说遗迹", "请选择目标", theme::kPurple); break; }
      const RuinsTargetSpec spec = ruinsTargetAt(index);
      const bool captured = save.captureCounts[spec.species - 1] != 0;
      title(speciesName(spec.species),
            s.command == CommandResult::InvalidPhase ? "先完成当前遭遇" :
            captured ? "已完成 · 重打无奖励" : "首领战", theme::kGold);
      thumbnail(spec.species, 233, 118, 1);
      specLine(spec.battle, 158);
      break;
    }
    default: break;
  }
  menu(s, e);
}

void Screens::ruinsResult(const ScreenState& s, const Engine& e, uint32_t now) {
  (void)s;
  (void)now;
  const auto& save = e.view();
  const Encounter& encounter = save.encounter;
  char text[72];
  const uint8_t clue = ruinsClueIndexForEncounter(encounter);
  if (clue != kRuinsNoIndex) {
    const RuinsClueSpec spec = ruinsClueAt(clue);
    if (save.result == Result::RuinsClue) {
      title("线索已获得", routeName(spec.route), theme::kGold);
      // The card hands over the route-order slot as soon as the clue is earned.
      snprintf(text, sizeof(text), "第%u位 · 线索 %u / %u", unsigned(ruinsClueOrderPosition(clue)),
               unsigned(clueCount(save)), unsigned(kRuinsClueTotal));
      line(text, 168, theme::kText);
      line(clueCount(save) == kRuinsClueTotal ? "可以排列路线" : "继续寻找线索", 198,
           clueCount(save) == kRuinsClueTotal ? theme::kGold : theme::kMuted);
    } else {
      title("线索未获得", routeName(spec.route), theme::kRed);
      snprintf(text, sizeof(text), "任务 · %s", clueTask(clue));
      line(text, 160, theme::kGold);
      line("其他线索保留", 190, theme::kMuted);
      line("仅重试本次线索", 216, theme::kMuted);
    }
    return;
  }
  if (!ruinsTargetExists(encounter.species)) {
    title("传说遗迹", "", theme::kPurple);
    return;
  }
  const RuinsTargetSpec spec = ruinsTargetAt(ruinsTargetIndex(encounter.species));
  if (save.result == Result::RuinsCleared) {
    title("已完成 · 无奖励", speciesName(spec.species), theme::kGold);
    thumbnail(spec.species, 233, 160, 2);
    line("不改变图鉴与捕获次数", 258, theme::kText);
    line("不消耗精灵球", 286, theme::kMuted);
    return;
  }
  const char* heading = save.result == Result::Lost ? "首领战失败" :
                        save.result == Result::Escaped ? "首领逃走了" :
                        save.result == Result::Released ? "已放弃捕捉" : "已经撤退";
  title(heading, "仅重试当前首领", save.result == Result::Lost ? theme::kRed : theme::kMuted);
  thumbnail(spec.species, 233, 150, 2);
  line("线索与解锁保留", 226, theme::kText);
}

bool Screens::exploration(const Engine& engine, uint32_t now) {
  const auto& save = engine.view();
  scene(save.route, false, 0, sceneSteps_);
  actorShadow(233, 238);
  uint8_t maxScale = 2;
  if (save.partner) {
    const auto& partner = data::kSprites[save.partner - 1];
    maxScale = displayScale(partner) == 3 && partner.height * 3 > 190 ? 2 : 3;
  }
  const bool rendered = save.partner && sprite(save.partner, now, homePlayback_, 248, 233, 0, maxScale);
  if (!rendered) line("伙伴画面不可用", 233, theme::kRed);
  title(speciesName(save.partner), "携带中", theme::routeColor(save.route));
  M5.Display.fillRoundRect(76, 68, 69, 26, 7, theme::kPanel);
  glyph(theme::Icon::Route, 91, 81, theme::routeColor(save.route));
  smallLabel(routeName(save.route), 107, 81, theme::routeColor(save.route), middle_left);
  ballCount(availableBalls(save), 81, 333);
  progress(save.explorationCredit, kEncounterSteps, 361, theme::routeColor(save.route));
  const auto& clock = save.clock;
  const bool todayKnown = clock.trusted && clock.anchor.trusted && !clock.pending.active && !clock.anomaly &&
    save.today.dayKey && save.today.dayKey == clock.anchor.dayKey && save.today.epoch == clock.calibrationEpoch;
  char text[96];
  if (todayKnown) {
    formatSteps(save.today.steps, text, sizeof(text));
    todayNumber(text, !save.today.complete);
  } else {
    line("今日 --", 393, theme::kText, true);
  }
  if (engine.stepsIncomplete()) line("部分步数缺失", 428, theme::kRed);
  else if (engine.loadResult().degraded) line("已从备用存档恢复", 428, theme::kGold);
  else if (save.phase == Phase::EncounterReady) line("发现宝可梦 · 侧键打开菜单", 428, theme::kGold);
  else if (save.phase == Phase::ThrowReady) line("待捕捉 · 侧键打开菜单", 428, theme::kGold);
  else line("左 / 右键打开菜单", 428, theme::kMuted);
  return rendered;
}
} }
