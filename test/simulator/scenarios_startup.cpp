#include "driver.hpp"

namespace simulator { namespace e2e {
void scenarioStartup(Driver& d, const std::string& phase) {
  d.check(phase == "new" || phase == "reload" || phase == "error", "known startup phase");
  d.check(d.runtime().startupActive() && d.now() < 1200, "cold boot starts with splash");
  const auto destination = d.screen().page;
  const auto steps = d.save().totalSteps;
  d.shot("startup-first");
  d.key(SDL_SCANCODE_RETURN, true);
  d.mouse(233, 233, true);
  d.advance(1180 - d.now());
  d.check(d.runtime().startupActive(), "splash remains visible for its minimum duration");
  d.check(d.save().totalSteps == steps, "startup does not add walking progress");
  d.until([&] { return !d.runtime().startupActive(); }, 1000, "splash completes automatically");
  d.key(SDL_SCANCODE_RETURN, false);
  d.mouse(233, 233, false);
  d.check(d.screen().page == destination && d.screen().selected == 0, "held key and touch never click through");
  if (phase == "error") {
    d.check(d.screen().page == Page::Error && !d.runtime().engine().ready(), "corrupt save shows error");
    d.select(1);
    d.check(d.screen().page == Page::Viewer, "fresh input opens Pokedex after storage error");
  } else {
    if (phase == "new") bootNewSave(d);
    d.check(d.screen().page == Page::Home && d.save().partner == 1, "starter reaches home");
    d.homeMenu();
    d.back();
    if (phase == "new") d.walk(8);
    else d.check(d.save().totalSteps >= 8, "walking progress survives a new process with splash");
    d.state("startup-end");
  }
  d.close();
}
} }
