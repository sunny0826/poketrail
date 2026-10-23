#include "screens.hpp"
#include "encounter_catalog.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
const char* progressionMessage(CommandResult result) {
  switch (result) {
    case CommandResult::TargetOwned: return "已拥有此形态";
    case CommandResult::InsufficientProgress: return "培养步数还不够";
    case CommandResult::Stale: return "伙伴已变化，请重新选择";
    case CommandResult::InvalidPhase: return "先携带它并完成当前流程";
    case CommandResult::NoBalls: return "暂无精灵球，继续走路获取";
    case CommandResult::SavePending: case CommandResult::StorageError: return "等待保存成功";
    case CommandResult::InvalidInput: return "当前无法执行";
    case CommandResult::Applied: case CommandResult::Duplicate: return "";
    default: return "稍后再试";
  }
}
void Screens::progression(const ScreenState& s, const Engine& e) {
  char text[120];
  const Save& save = e.view();
  if (s.page == Page::ReleaseConfirm) {
    line("放弃这次捕捉？", 66, TFT_WHITE, true);
    line(speciesName(save.encounter.species), 112, TFT_CYAN);
    line("不退探索步数或已用精灵球", 158);
    line("伙伴与已有收藏保留", 188);
    item(s, 0, "确认放弃"); item(s, 1, "返回投球");
    return;
  }
  if (s.page == Page::EvolutionConfirm) {
    const EvolutionRule* rule = evolutionRule(s.evolution.from, s.evolution.to);
    line("确认进化", 44, TFT_WHITE, true);
    snprintf(text, sizeof(text), "%s → %s", speciesName(s.evolution.from), speciesName(s.evolution.to));
    line(text, 84, TFT_CYAN);
    thumbnail(s.evolution.from, 173, 128, 2);
    thumbnail(s.evolution.to, 293, 128, 2);
    snprintf(text, sizeof(text), "使用 %lu 培养步", static_cast<unsigned long>(rule ? rule->steps : 0));
    line(text, 180);
    line("原形态不再可携带", 196, TFT_YELLOW);
    item(s, 0, "确认进化"); item(s, 1, "稍后");
    line("余下培养步留在原形态", 314);
    if (s.evolution.from && save.captureCounts[s.evolution.from - 1])
      line("已捕获的原形态不会再遇到", 342, TFT_YELLOW);
    if (s.command != CommandResult::Applied) line(progressionMessage(s.command), 370, TFT_YELLOW);
    return;
  }
  const EvolutionView view = e.evolutionView(s.species);
  line(speciesName(s.species), 42, TFT_WHITE, true);
  thumbnail(s.species, 233, 89, 2);
  snprintf(text, sizeof(text), "陪伴 %llu · 可用 %llu 步", static_cast<unsigned long long>(view.walked),
           static_cast<unsigned long long>(view.available));
  line(text, 140);
  if (s.evolved) line("进化已保存", 170, TFT_CYAN);
  else if (s.command != CommandResult::Applied && s.command != CommandResult::Duplicate)
    line(s.command == CommandResult::InvalidInput && s.species != save.partner ?
         "先携带这位伙伴" : progressionMessage(s.command), 170, TFT_YELLOW);
  else if (!view.count) line("已是最终形态", 170, TFT_DARKGREY);
  else {
    const EvolutionOption& option = view.options[s.selected > 0 && s.selected <= view.count ? s.selected - 1 : 0];
    snprintf(text, sizeof(text), "%s需 %lu 步", speciesName(option.target), static_cast<unsigned long>(option.cost));
    line(text, 170, TFT_YELLOW);
  }
  item(s, 0, save.partner == s.species ? "已携带" : "选择携带");
  for (unsigned i = 0; i < view.count; ++i) {
    const EvolutionOption& option = view.options[i];
    const char* suffix = option.status == CommandResult::TargetOwned ? " · 已拥有" :
      option.status == CommandResult::InsufficientProgress ? " · 未达标" : "";
    snprintf(text, sizeof(text), "%s%s", speciesName(option.target), suffix);
    item(s, i + 1, text, menu_layout::kTop, option.target);
  }
  item(s, view.count + 1, "返回伙伴列表");
}
} }
