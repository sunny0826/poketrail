#include "progression_support.hpp"
#include "viewer_screen.hpp"
#include "../support/rendered_text.hpp"

namespace simulator { namespace e2e {
using pokedex::i18n::Language;
namespace {
void text(Driver& d, const char* expected, uint16_t color = theme::kText) {
  d.frames();
  d.check(rendered_text::contains(expected, color), std::string("English page renders ") + expected);
}
void viewerSpecies(Driver& d, uint16_t id, const char* name, const char* types) {
  const unsigned forward = (id + kSpeciesCount - pokedex::viewer_screen::species()) % kSpeciesCount;
  const auto direction = forward <= kSpeciesCount / 2 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT;
  for (unsigned n = 0; pokedex::viewer_screen::species() != id && n < kSpeciesCount; ++n)
    d.press(direction);
  d.check(pokedex::viewer_screen::species() == id, "real viewer keys reach named species");
  text(d, name); text(d, types, theme::kBlue);
  d.shot(std::string("english-viewer-") + std::to_string(id));
}
void pages(Driver& d) {
  d.check(d.runtime().language() == Language::English, "English fixture is restored before first frame");
  text(d, "Bulbasaur");
  d.homeMenu(); text(d, "My partners"); d.shot("english-menu");
  d.select(4);
  viewerSpecies(d, 1, "Bulbasaur", "Grass / Poison");
  viewerSpecies(d, 29, "Nidoran♀", "Poison");
  viewerSpecies(d, 32, "Nidoran♂", "Poison");
  viewerSpecies(d, 83, "Farfetch’d", "Normal / Flying");
  viewerSpecies(d, 122, "Mr. Mime", "Psychic / Fairy");
  d.back(); d.homeMenu(); d.select(2);
  d.select(2); text(d, "Kangaskhan"); d.shot("english-long-partner"); d.back();
  d.select(6); d.check(d.screen().species == 649, "last owned partner remains reachable");
  text(d, "Genesect"); d.shot("english-last-partner"); d.back();
  d.swipe(320, 140); d.swipe(320, 140);
  d.check(d.screen().selected == 6, "partner list reaches final page by touch");
  d.shot("english-partners-last-page");
  text(d, "Back"); d.select(7);
  d.check(d.screen().page == Page::Menu, "partner last-page Back returns to menu");
  d.select(5); text(d, "Stats and records"); d.select(1);
  d.check(d.screen().page == Page::Clock, "English menu opens clock");
  d.shot("english-clock"); text(d, "Date and time");
  d.homeMenu(); d.select(2); d.select(0); d.select(1);
  d.check(d.screen().page == Page::EvolutionConfirm, "English partner opens evolution confirmation");
  text(d, "Confirm evolution"); text(d, "Use 7500 training steps");
  d.shot("english-evolution-confirm"); d.select(0);
  d.check(d.save().partner == 2 && d.save().progress[0].spent == 7500 &&
          d.save().progress[1].walked == 200, "English evolution preserves training-account rules");
  progression_support::finishEvolution(d, "English evolution");
  text(d, "Ivysaur"); d.shot("english-evolution-saved");
}
}
void scenarioLanguagePages(Driver& d, const std::string& phase) {
  if (phase == "run") pages(d);
  else {
    d.check(d.runtime().language() == Language::English && d.save().partner == 2 &&
            d.save().progress[0].spent == 7500 && d.save().progress[1].walked == 200,
            "English evolution and exact debit survive new process");
    text(d, "Ivysaur"); d.shot("english-pages-reload");
  }
  d.state("language-final"); d.close();
}
} }
