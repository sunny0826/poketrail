#include "driver.hpp"
#include "board.hpp"

namespace simulator { namespace e2e {
void scenarioStartup(Driver& d, const std::string& phase) {
  d.check(d.runtime().startupActive() && d.now() < 1200, "cold boot starts with splash");
  const auto destination = d.screen().page;
  const auto sequence = d.save().sequence;
  const auto steps = d.save().totalSteps;
  const auto writes = d.writes();
  d.shot("startup-first");
  unsigned ink = 0;
  for (int y = 310; y < 355; ++y) for (int x = 100; x < 366; ++x)
    if (M5.Display.readPixel(x, y) == theme::kText) ++ink;
  d.check(ink > 500, "large PokeTrail title is rendered");
  // Hold real SDL controls across the transition; their release must be consumed.
  d.key(SDL_SCANCODE_RETURN, true);
  d.mouse(233, 233, true);
  d.advance(400);
  d.shot("startup-animation");
  d.advance(1180 - d.now());
  d.check(d.runtime().startupActive(), "splash remains visible for its minimum duration");
  d.check(d.writes() == writes && d.save().sequence == sequence && d.save().totalSteps == steps,
          "startup frames neither commit nor change gameplay state");
  d.frames(1);
  d.check(!d.runtime().startupActive() && d.now() == 1200, "splash completes at 1200ms without blocking event loop");
  d.check(d.screen().page == destination && !d.dimmed(), "startup preserves the correct destination");
  d.key(SDL_SCANCODE_RETURN, false);
  d.mouse(233, 233, false);
  d.check(d.screen().page == destination && d.screen().selected == 0, "held key and touch never click through");
  d.shot("startup-destination");
  if (phase == "error") {
    d.check(d.screen().page == Page::Error && !d.runtime().engine().ready(), "corrupt save shows error");
    d.check(d.writes() == 0, "corrupt save is not overwritten");
    d.select(1);
    d.check(d.screen().page == Page::Viewer, "fresh input can still open Pokedex after storage error");
  } else {
    if (phase == "new") bootNewSave(d);
    d.check(d.screen().page == Page::Home && d.save().partner == 1, "new or existing starter reaches home");
    d.homeMenu();
    d.check(d.screen().page == Page::Menu, "fresh input works after startup");
    d.back();
    if (phase == "new" || phase == "existing") d.walk(8);
    else d.check(d.save().totalSteps >= 8, "walking progress survives a new process with splash");
    d.state("startup-end");
    d.shot("startup-home");
  }
  d.close();
}
} }
