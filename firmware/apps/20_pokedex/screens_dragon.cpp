#include "screens.hpp"
#include "board.hpp"
#include "sprites_generated.hpp"
#include <cstdio>

namespace pokedex { namespace ui {
using namespace adventure;

void Screens::dragon(const ScreenState& s, const Engine& e, uint32_t now) {
  (void)now;
  const auto& save = e.view();
  const DragonTrialState& trial = save.dragonTrial;
  char text[96];
  switch (s.page) {
    case Page::Dragon: {
      const uint8_t need = dragonCapturesUntilOpen(save);
      if (dragonTicketOpen(save)) title("龙穴试炼", "捕获 6 的倍数 · 本轮开放", theme::kPurple);
      else {
        snprintf(text, sizeof(text), "还差 %u 次捕获", unsigned(need));
        title("龙穴试炼", text, theme::kPurple);
      }
      snprintf(text, sizeof(text), "已捕获 %llu 次",
               static_cast<unsigned long long>(dragonCaptureTotal(save)));
      line(text, 124, theme::kMuted);
      break;
    }
    case Page::DragonStage: {
      const uint8_t count = dragonTrialStageCount(trial.trialId);
      const DragonStageSpec stage = dragonStageAt(trial.trialId, trial.stageIndex);
      snprintf(text, sizeof(text), "第 %u / %u 阶段", unsigned(trial.stageIndex) + 1,
               unsigned(count));
      title(speciesName(dragonTrialSpecies(trial.trialId)), text, theme::kPurple);
      thumbnail(dragonTrialSpecies(trial.trialId), 233, 120);
      snprintf(text, sizeof(text), "生命 我方%u / %u", unsigned(trial.stageStartHp),
               unsigned(dragongen::kPlayerHp));
      line(text, 154, theme::kText);
      snprintf(text, sizeof(text), "敌方 %u · 节拍 %.2f 秒", unsigned(stage.enemyHp),
               double(stage.beatMs) / 1000.0);
      line(text, 176, theme::kMuted);
      if (trial.guard)
        snprintf(text, sizeof(text), "窗口 %u/%ums · 护盾 1",
                 unsigned(dragonNormalWindow(trial.normalBonus)),
                 unsigned(dragonPerfectWindow(trial.perfectBonus)));
      else
        snprintf(text, sizeof(text), "窗口 %u/%ums",
                 unsigned(dragonNormalWindow(trial.normalBonus)),
                 unsigned(dragonPerfectWindow(trial.perfectBonus)));
      line(text, 194, theme::kMuted);
      break;
    }
    case Page::DragonUpgrade: {
      // The snapshot already points at the next stage, so the completed one is
      // one lower and its carried HP is already saved.
      snprintf(text, sizeof(text), "第 %u 阶段完成", unsigned(trial.stageIndex));
      title("选择强化", text, theme::kGold);
      snprintf(text, sizeof(text), "生命 我方%u / %u · 窗口 %u/%ums", unsigned(trial.stageStartHp),
               unsigned(dragongen::kPlayerHp), unsigned(dragonNormalWindow(trial.normalBonus)),
               unsigned(dragonPerfectWindow(trial.perfectBonus)));
      line(text, 150, theme::kText);
      snprintf(text, sizeof(text), "下一阶段 · 第 %u 阶段", unsigned(trial.stageIndex) + 1);
      line(text, 176, theme::kMuted);
      break;
    }
    default: break;
  }
  menu(s, e);
}

void Screens::dragonResult(const ScreenState& s, const Engine& e, uint32_t now) {
  (void)s;
  (void)now;
  const auto& save = e.view();
  const Encounter& encounter = save.encounter;
  const DragonTrialState& trial = save.dragonTrial;
  const uint8_t count = dragonTrialStageCount(trial.trialId);
  char text[96];
  switch (save.result) {
    case Result::Lost:
      snprintf(text, sizeof(text), "第 %u / %u 阶段 · 可重试", unsigned(trial.stageIndex) + 1,
               unsigned(count));
      title("试炼失败", text, theme::kRed);
      break;
    case Result::Retreated:
      snprintf(text, sizeof(text), "第 %u / %u 阶段 · 可重试", unsigned(trial.stageIndex) + 1,
               unsigned(count));
      title("已经撤退", text, theme::kMuted);
      break;
    case Result::Escaped:
      title("未能捕获", "最终阶段 · 可重试", theme::kMuted);
      break;
    case Result::Released:
      title("已放弃捕捉", "最终阶段 · 可重试", theme::kMuted);
      break;
    case Result::DragonCleared:
      title("已完成 · 无奖励", speciesName(encounter.species), theme::kGold);
      break;
    default:
      title("试炼结束", speciesName(encounter.species), theme::kMuted);
      break;
  }
  thumbnail(encounter.species, 233, 150, 2);
  if (save.result == Result::DragonCleared) {
    line("不改变图鉴与捕获次数", 226, theme::kText);
    line("不消耗精灵球", 254, theme::kMuted);
  } else {
    line("生命与强化均已保留", 226, theme::kText);
  }
}
} }
