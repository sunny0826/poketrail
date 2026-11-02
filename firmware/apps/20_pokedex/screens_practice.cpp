#include "screens.hpp"
#include "board.hpp"
#include "encounter_catalog.hpp"
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
void Screens::practiceThrow(const ScreenState& s, uint32_t now) {
  scene(regionOf(s.practiceSpecies), true);
  title(speciesName(s.practiceSpecies), "", theme::kGold);
  if (s.capture.active) { capture(s, s.practiceSpecies, now); return; }
  ballSlots(s.practiceUsedBalls);
  if (s.practiceThrowing) {
    aim(s.practiceSpecies, now, true, int32_t(s.practiceTargetMs - now), s.charging, now - s.chargeStartMs);
    line(tr(Text::ReleaseThrowCost), 399, theme::kMint);
    line(tr(Text::PracticeNoCollection), 426, theme::kMuted);
    return;
  }
  aim(s.practiceSpecies, now, false, 0, false, 0);
  line(tr(Text::HoldThrow), 399, theme::kMint);
  line(tr(Text::EndPracticeTop), 426, theme::kMuted);
}
void Screens::practiceResult(const ScreenState& s, uint32_t now) {
  title(s.practiceResult == Result::Captured ? tr(Text::PracticeSuccess) :
        s.practiceResult == Result::Lost ? tr(Text::BattleLost) : tr(Text::PracticeEnded),
        speciesName(s.practiceSpecies), theme::kGold);
  if (s.practiceResult == Result::Captured) {
    actorShadow(233, 168);
    sprite(s.practiceSpecies, now, resultPlayback_, 168, 233, 0, 2, 120);
    line(tr(Text::NotCollected), 262, theme::kMuted);
  } else {
    thumbnail(s.practiceSpecies, 233, 160, 2);
    line(s.practiceResult == Result::Lost ? tr(Text::BattleTryAgain) : tr(Text::PokemonFled), 240, theme::kMuted);
  }
  item(s, 0, tr(Text::PracticeAgain), menu_layout::kTop);
  item(s, 1, tr(Text::ReturnExploration), menu_layout::kTop);
}
} }
