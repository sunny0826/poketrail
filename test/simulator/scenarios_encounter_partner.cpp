#include "driver.hpp"
#include "progression_support.hpp"
#include "../support/rendered_text.hpp"

namespace simulator { namespace e2e {
namespace {
void startWith(Driver& d, uint16_t partner, const std::string& tag) {
  progression_support::toHome(d, tag);
  const Encounter before = d.save().encounter;
  const uint64_t spent = d.save().explorationSpent;
  d.homeMenu(); d.select(MenuItem::Encounter); d.select(0); d.frames();
  const Encounter& after = d.save().encounter;
  d.check(d.screen().page == Page::Battle && after.partner == partner,
          "battle uses the current carried partner");
  d.check(after.id == before.id && after.species == before.species &&
          after.seed == before.seed && after.route == before.route &&
          after.ruleVersion == before.ruleVersion && d.save().explorationSpent == spent,
          "binding a partner preserves the opponent, rules and charged encounter");
  d.check(rendered_text::contains(pokedex::i18n::speciesName(d.runtime().language(), partner), theme::kText),
          "battle renders the current partner name");
  d.shot(tag); d.state(tag);
}
void missAndFinish(Driver& d) {
  d.throwBall(false); d.finishCapture();
  d.throwBall(false); d.finishCapture();
  d.check(d.screen().page == Page::Result && d.save().result == Result::Escaped,
          "two misses finish the bound encounter without a capture");
  acknowledge(d);
}
}

void scenarioEncounterPartner(Driver& d, const std::string& phase) {
  if (phase == "run") {
    d.walk(1);
    d.check(d.save().phase == Phase::EncounterReady && d.save().encounterQueueSize > 0 &&
            d.save().encounter.partner == 1 && d.save().encounterQueue[0].partner == 1,
            "walking with Bulbasaur creates a current encounter and a queued encounter");
    progression_support::carry(d, 1, 4, "switch before battle");
    startWith(d, 4, "switched-charmander");
    d.winBattle();
    d.check(d.save().phase == Phase::ThrowReady && d.save().encounter.partner == 4,
            "victory saves the selected battle partner");
  } else if (phase == "reload") {
    d.check(d.save().phase == Phase::ThrowReady && d.save().partner == 4 &&
            d.save().encounter.partner == 4 && d.save().encounter.id == 1,
            "new process restores the won encounter with Charmander");
    d.check(d.screen().page == Page::Throw, "saved victory resumes on the throw page");
    missAndFinish(d);
    d.check(d.save().encounter.id == 2 && d.save().encounter.partner == 1,
            "FIFO promotes the encounter originally queued with Bulbasaur");
    progression_support::carry(d, 0, 1, "carry evolution source");
    progression_support::homeEvolution(d, 1, 2, "evolve before queued battle");
    d.select(0); progression_support::finishEvolution(d, "evolve queued partner");
    d.check(!owns(d.save(), 1) && d.save().partner == 2,
            "queued encounter's original partner has evolved and left the roster");
    startWith(d, 2, "queued-ivysaur");
    d.tap(233, 35);
    d.until([&] { return d.screen().page == Page::Pause; }, 5000, "pause saves the battle");
    d.check(d.save().encounter.partner == 2, "paused encounter keeps its bound partner");
  } else if (phase == "paused-reload") {
    d.check(d.save().phase == Phase::EncounterReady && d.save().encounter.id == 2 &&
            d.save().partner == 2 && d.save().encounter.partner == 2,
            "new process restores the paused encounter as ready with its saved partner");
    progression_support::carry(d, 1, 4, "switch before restarting battle");
    startWith(d, 4, "restarted-charmander");
    d.winBattle();
    d.fault(Fault::BeforeWrite); d.throwBall(false);
    d.check(d.screen().page == Page::Error && d.save().encounter.partner == 4 &&
            d.save().encounter.usedBalls == 0, "failed throw preserves the battle partner and attempt count");
    d.fault(Fault::None); d.select(0);
    d.check(d.save().encounter.partner == 4 && d.save().encounter.usedBalls == 1,
            "retry commits one throw for the same partner");
  } else if (phase == "settled-reload") {
    d.check(d.save().phase == Phase::ThrowReady && d.save().encounter.id == 2 &&
            d.save().partner == 4 && d.save().encounter.partner == 4 &&
            d.save().encounter.usedBalls == 1 && d.save().progress[0].spent == 7500,
            "new process restores the retried throw and evolution debit once");
    d.check(d.screen().page == Page::Throw, "saved victory resumes on the throw page");
    d.throwBall(false); d.finishCapture();
    d.check(d.screen().page == Page::Result && d.save().encounter.partner == 4 &&
            d.save().result == Result::Escaped, "result retains the partner used in battle");
  } else d.check(false, "unknown encounter partner phase");
  d.state("partner-end"); d.close();
}
} }
