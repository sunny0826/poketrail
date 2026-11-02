#include "screens.hpp"
#include "encounter_catalog.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
const char* progressionMessage(i18n::Language language, CommandResult result) {
  switch (result) {
    case CommandResult::TargetOwned: return i18n::text(language, Text::TargetOwned);
    case CommandResult::InsufficientProgress: return i18n::text(language, Text::InsufficientProgress);
    case CommandResult::Stale: return i18n::text(language, Text::PartnerChanged);
    case CommandResult::InvalidPhase: return i18n::text(language, Text::FinishAndCarry);
    case CommandResult::NoBalls: return i18n::text(language, Text::LegacyBalls);
    case CommandResult::SavePending: case CommandResult::StorageError: return i18n::text(language, Text::WaitForSave);
    case CommandResult::InvalidInput: return i18n::text(language, Text::CannotAct);
    case CommandResult::Applied: case CommandResult::Duplicate: return "";
    default: return i18n::text(language, Text::TryLater);
  }
}
void Screens::progression(const ScreenState& s, const Engine& e) {
  char text[120];
  const Save& save = e.view();
  if (s.page == Page::EvolutionConfirm) {
    const EvolutionRule* rule = evolutionRule(s.evolution.from, s.evolution.to);
    title(tr(Text::ConfirmEvolution), "", theme::kGold);
    snprintf(text, sizeof(text), "%s → %s", speciesName(s.evolution.from), speciesName(s.evolution.to));
    line(text, 80, theme::kGold);
    thumbnail(s.evolution.from, 164, 127, 2);
    line("→", 127, theme::kGold, true);
    thumbnail(s.evolution.to, 302, 127, 2);
    snprintf(text, sizeof(text), tr(Text::EvolutionCost), static_cast<unsigned long>(rule ? rule->steps : 0));
    line(text, 168, theme::kText);
    line(tr(Text::OriginalUnavailable), 193, theme::kGold);
    item(s, 0, tr(Text::ConfirmEvolution), menu_layout::kTop);
    item(s, 1, tr(Text::Later), menu_layout::kTop);
    line(tr(Text::RemainderStays), 350, theme::kMuted);
    if (s.evolution.from && save.captureCounts[s.evolution.from - 1])
      line(tr(Text::CapturedNeverReturns), 376, theme::kGold);
    if (s.command != CommandResult::Applied) line(progressionMessage(s.command), 402, theme::kGold);
    return;
  }
  const EvolutionView view = e.evolutionView(s.species);
  title(speciesName(s.species), "", theme::kMint);
  snprintf(text, sizeof(text), tr(Text::WalkedAvailable), static_cast<unsigned long long>(view.walked),
           static_cast<unsigned long long>(view.available));
  M5.Display.setFont(&fonts::efontCN_16);
  if (M5.Display.textWidth(text) <= 300) line(text, 80, theme::kMint);
  else {
    snprintf(text, sizeof(text), tr(Text::WalkedSteps), static_cast<unsigned long long>(view.walked));
    line(text, 80, theme::kMint);
    snprintf(text, sizeof(text), tr(Text::AvailableSteps), static_cast<unsigned long long>(view.available));
    line(text, 165, theme::kMint);
  }
  thumbnail(s.species, 233, 132, 2);
  if (s.evolved) alert(tr(Text::EvolutionSaved), 188, theme::kMint);
  else if (s.command != CommandResult::Applied && s.command != CommandResult::Duplicate)
    alert(s.command == CommandResult::InvalidInput && s.species != save.partner ?
          tr(Text::CarryFirst) : progressionMessage(s.command), 188, theme::kGold);
  else if (!view.count) alert(tr(Text::FinalFormReady), 188, theme::kMuted);
  else {
    const EvolutionOption& option = view.options[s.selected > 0 && s.selected <= view.count ? s.selected - 1 : 0];
    snprintf(text, sizeof(text), tr(Text::EvolutionNeeds), speciesName(option.target), static_cast<unsigned long>(option.cost));
    alert(text, 188, theme::kGold);
  }
  item(s, 0, save.partner == s.species ? tr(Text::Carried) : tr(Text::SelectPartner), menu_layout::kTop);
  for (unsigned i = 0; i < view.count; ++i) {
    const EvolutionOption& option = view.options[i];
    const char* note = option.status == CommandResult::TargetOwned ? tr(Text::OwnedNote) :
      option.status == CommandResult::InsufficientProgress ? tr(Text::NotReady) : nullptr;
    item(s, i + 1, speciesName(option.target), menu_layout::kTop, option.target, false, note);
  }
  item(s, view.count + 1, tr(Text::BackToPartners), menu_layout::kTop);
}
} }
