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
    if (s.evolution.from && save.captureCounts[s.evolution.from - 1])
      line(tr(Text::CapturedNeverReturns), 350, theme::kGold);
    if (s.command != CommandResult::Applied) line(progressionMessage(s.command), 376, theme::kGold);
    return;
  }
  const bool carried = s.page == Page::CarriedPartner;
  const uint16_t species = carried ? save.partner : s.species;
  if (carried && !species) {
    title(tr(Text::CarriedPartner), "", theme::kMint);
    line(tr(Text::NoCarriedPartner), 150, theme::kMuted);
    item(s, 0, tr(Text::BackToMenu));
    return;
  }
  const EvolutionView view = e.evolutionView(species);
  title(speciesName(species), "", theme::kMint);
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
  thumbnail(species, 233, 132, 2);
  if (s.evolved) alert(tr(Text::EvolutionSaved), 188, theme::kMint);
  else if (!carried && s.command != CommandResult::Applied && s.command != CommandResult::Duplicate)
    alert(s.command == CommandResult::InvalidInput && s.species != save.partner ?
          tr(Text::CarryFirst) : progressionMessage(s.command), 188, theme::kGold);
  else if (!view.count) alert(tr(Text::FinalFormReady), 188, theme::kMuted);
  else {
    const EvolutionOption& option = view.options[s.selected > 0 && s.selected <= view.count ? s.selected - 1 : 0];
    snprintf(text, sizeof(text), tr(Text::EvolutionNeeds), speciesName(option.target), static_cast<unsigned long>(option.cost));
    alert(carried && option.status != CommandResult::Applied ? progressionMessage(option.status) : text,
          188, theme::kGold);
  }
  item(s, 0, carried ? tr(Text::SwitchPartner) : save.partner == species ? tr(Text::Carried) : tr(Text::SelectPartner), menu_layout::kTop);
  for (unsigned i = 0; i < view.count; ++i) {
    const EvolutionOption& option = view.options[i];
    const char* note = option.status == CommandResult::TargetOwned ? tr(Text::OwnedNote) :
      option.status == CommandResult::InsufficientProgress ? tr(Text::NotReady) : nullptr;
    if (carried) {
      snprintf(text, sizeof(text), "%lu %s", static_cast<unsigned long>(option.cost), tr(Text::Steps));
      note = text;
    }
    item(s, i + 1, speciesName(option.target), menu_layout::kTop, option.target, false, note,
         carried && option.status != CommandResult::Applied);
  }
  item(s, view.count + 1, tr(carried ? Text::BackToMenu : Text::BackToPartners), menu_layout::kTop);
}
void Screens::drawProgressionRow(const ScreenState& s, const Engine& e, unsigned index, int top) {
  if (s.page == Page::EvolutionConfirm) {
    item(s, index, index == 0 ? tr(Text::ConfirmEvolution) : tr(Text::Later), top);
    return;
  }
  const Save& save = e.view();
  const bool carried = s.page == Page::CarriedPartner;
  const uint16_t species = carried ? save.partner : s.species;
  if (carried && !species) { item(s, 0, tr(Text::BackToMenu), top); return; }
  const EvolutionView view = e.evolutionView(species);
  if (index == 0) {
    item(s, 0, carried ? tr(Text::SwitchPartner) : save.partner == species ? tr(Text::Carried) : tr(Text::SelectPartner), top);
    return;
  }
  if (index <= view.count) {
    char text[40];
    const EvolutionOption& option = view.options[index - 1];
    const char* note = option.status == CommandResult::TargetOwned ? tr(Text::OwnedNote) :
      option.status == CommandResult::InsufficientProgress ? tr(Text::NotReady) : nullptr;
    if (carried) {
      snprintf(text, sizeof(text), "%lu %s", static_cast<unsigned long>(option.cost), tr(Text::Steps));
      note = text;
    }
    item(s, index, speciesName(option.target), top, option.target, false, note,
         carried && option.status != CommandResult::Applied);
    return;
  }
  item(s, view.count + 1, tr(carried ? Text::BackToMenu : Text::BackToPartners), top);
}
void Screens::drawProgression(const ScreenState& s, const Engine& e, bool full) {
  const Save& save = e.view();
  if (s.page == Page::EvolutionConfirm) {
    const bool fromCap = s.evolution.from && save.captureCounts[s.evolution.from - 1];
    if (full) {
      progression(s, e);
    } else if (s.evolution.from != snap_.evoFrom || s.evolution.to != snap_.evoTo ||
               fromCap != snap_.firstCapture || s.command != snap_.command) {
      WriteScope sc;
      clearRegion(0, 36, 468, 405, theme::kBackground);
      progression(s, e);
      glyph(theme::Icon::Back, 112, 52, theme::kMuted);
    } else {
      updateListRows(s, e, menu_layout::kTop, false, 2, &Screens::drawProgressionRow);
    }
    snap_.evoFrom = s.evolution.from;
    snap_.evoTo = s.evolution.to;
    snap_.firstCapture = fromCap;
    snap_.command = s.command;
    return;
  }
  const bool carried = s.page == Page::CarriedPartner;
  const uint16_t species = carried ? save.partner : s.species;
  if (carried && !species) {
    if (full || snap_.species != 0) {
      if (!full) {
        WriteScope sc;
        clearRegion(0, 36, 468, 405, theme::kBackground);
        progression(s, e);
        glyph(theme::Icon::Back, 112, 52, theme::kMuted);
      } else {
        progression(s, e);
      }
    }
    snap_.species = 0;
    return;
  }
  const EvolutionView view = e.evolutionView(species);
  uint8_t alertKind = 4;
  uint16_t alertTarget = 0;
  uint32_t alertCost = 0;
  CommandResult alertStatus = CommandResult::Applied;
  if (s.evolved) alertKind = 1;
  else if (!carried && s.command != CommandResult::Applied && s.command != CommandResult::Duplicate) {
    alertKind = 2; alertStatus = s.command; alertTarget = save.partner;
  } else if (!view.count) alertKind = 3;
  else {
    const EvolutionOption& opt = view.options[s.selected > 0 && s.selected <= view.count ? s.selected - 1 : 0];
    alertTarget = opt.target; alertCost = opt.cost; alertStatus = opt.status;
  }
  if (full) {
    progression(s, e);
  } else if (species != snap_.species || save.partner != snap_.partner ||
             save.rosterRevision != snap_.rosterRevision || view.walked != snap_.walked ||
             view.available != snap_.available || s.evolved != snap_.evolved ||
             (!carried && s.command != snap_.command)) {
    WriteScope sc;
    clearRegion(0, 36, 468, 405, theme::kBackground);
    progression(s, e);
    if (2 + view.count > menu_layout::kPageSize) pager(s, 2 + view.count);
    glyph(theme::Icon::Back, 112, 52, theme::kMuted);
  } else {
    if (alertKind != snap_.alertKind || alertTarget != snap_.alertTarget ||
        alertCost != snap_.alertCost || alertStatus != snap_.alertStatus) {
      WriteScope sc;
      clearRegion(55, 172, 358, 32, theme::kBackground);
      char text[120];
      const EvolutionOption& option = view.options[s.selected > 0 && s.selected <= view.count ? s.selected - 1 : 0];
      snprintf(text, sizeof(text), tr(Text::EvolutionNeeds), speciesName(option.target),
               static_cast<unsigned long>(option.cost));
      alert(carried && option.status != CommandResult::Applied ? progressionMessage(option.status) : text,
            188, theme::kGold);
    }
    updateListRows(s, e, menu_layout::kTop, true, 2 + view.count, &Screens::drawProgressionRow);
  }
  snap_.species = species;
  snap_.partner = save.partner;
  snap_.rosterRevision = save.rosterRevision;
  snap_.walked = view.walked;
  snap_.available = view.available;
  snap_.evolved = s.evolved;
  snap_.command = s.command;
  snap_.alertKind = alertKind;
  snap_.alertTarget = alertTarget;
  snap_.alertCost = alertCost;
  snap_.alertStatus = alertStatus;
}
} }
