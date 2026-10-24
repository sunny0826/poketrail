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
    title("放弃这次捕捉？", speciesName(save.encounter.species), theme::kRed);
    warning(233, 133, theme::kRed);
    line("不退探索步数或已用精灵球", 179, theme::kText);
    line("伙伴与已有收藏保留", 199, theme::kMuted);
    item(s, 0, "确认放弃", menu_layout::kTop);
    item(s, 1, "返回投球", menu_layout::kTop);
    return;
  }
  if (s.page == Page::EvolutionConfirm) {
    const EvolutionRule* rule = evolutionRule(s.evolution.from, s.evolution.to);
    title("确认进化", "", theme::kGold);
    snprintf(text, sizeof(text), "%s → %s", speciesName(s.evolution.from), speciesName(s.evolution.to));
    line(text, 80, theme::kGold);
    thumbnail(s.evolution.from, 164, 127, 2);
    line("→", 127, theme::kGold, true);
    thumbnail(s.evolution.to, 302, 127, 2);
    snprintf(text, sizeof(text), "使用 %lu 培养步", static_cast<unsigned long>(rule ? rule->steps : 0));
    line(text, 168, theme::kText);
    line("原形态不再可携带", 193, theme::kGold);
    item(s, 0, "确认进化", menu_layout::kTop);
    item(s, 1, "稍后", menu_layout::kTop);
    line("余下培养步留在原形态", 350, theme::kMuted);
    if (s.evolution.from && save.captureCounts[s.evolution.from - 1])
      line("已捕获的原形态不会再遇到", 376, theme::kGold);
    if (s.command != CommandResult::Applied) line(progressionMessage(s.command), 402, theme::kGold);
    return;
  }
  const EvolutionView view = e.evolutionView(s.species);
  title(speciesName(s.species), "", theme::kMint);
  snprintf(text, sizeof(text), "陪伴 %llu · 可用 %llu 步", static_cast<unsigned long long>(view.walked),
           static_cast<unsigned long long>(view.available));
  M5.Display.setFont(&fonts::efontCN_16);
  if (M5.Display.textWidth(text) <= 300) line(text, 80, theme::kMint);
  else {
    snprintf(text, sizeof(text), "陪伴 %llu 步", static_cast<unsigned long long>(view.walked));
    line(text, 80, theme::kMint);
    snprintf(text, sizeof(text), "可用 %llu 步", static_cast<unsigned long long>(view.available));
    line(text, 165, theme::kMint);
  }
  thumbnail(s.species, 233, 132, 2);
  if (s.evolved) alert("进化已保存", 188, theme::kMint);
  else if (s.command != CommandResult::Applied && s.command != CommandResult::Duplicate)
    alert(s.command == CommandResult::InvalidInput && s.species != save.partner ?
          "先携带这位伙伴" : progressionMessage(s.command), 188, theme::kGold);
  else if (!view.count) alert("已是最终形态", 188, theme::kMuted);
  else {
    const EvolutionOption& option = view.options[s.selected > 0 && s.selected <= view.count ? s.selected - 1 : 0];
    snprintf(text, sizeof(text), "%s需 %lu 步", speciesName(option.target), static_cast<unsigned long>(option.cost));
    alert(text, 188, theme::kGold);
  }
  item(s, 0, save.partner == s.species ? "已携带" : "选择携带", menu_layout::kTop);
  for (unsigned i = 0; i < view.count; ++i) {
    const EvolutionOption& option = view.options[i];
    const char* note = option.status == CommandResult::TargetOwned ? "已拥有" :
      option.status == CommandResult::InsufficientProgress ? "未达标" : nullptr;
    item(s, i + 1, speciesName(option.target), menu_layout::kTop, option.target, false, note);
  }
  item(s, view.count + 1, "返回伙伴列表", menu_layout::kTop);
}
} }
