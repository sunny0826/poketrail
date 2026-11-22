#include "driver.hpp"
#include "progression_support.hpp"
#include "../support/rendered_text.hpp"
#include <cstdio>

namespace simulator { namespace e2e {
namespace {
void counts(Driver& d, unsigned count, bool english = false) {
  d.check(pendingEncounterCount(d.save()) == count, "pending count excludes battles and finished encounters");
  progression_support::toHome(d, "queue count"); d.frames();
  char badge[40], menu[48];
  snprintf(badge, sizeof(badge), english ? "Encounters %u" : "遭遇%u", count);
  snprintf(menu, sizeof(menu), english ? "Resume encounter %u" : "继续遭遇%u", count);
  if (count) d.check(rendered_text::contains(badge, theme::kGold, 98, 125), "home shows committed encounter count");
  d.shot("queue-home-" + std::to_string(count) + (english ? "-en" : ""));
  d.homeMenu(); d.frames();
  char walking[48];
  snprintf(walking, sizeof(walking), english ? "Next encounter in %llu steps" : "下次遭遇还差 %llu 步",
           static_cast<unsigned long long>(d.save().explorationCredit < d.save().encounterTarget
             ? d.save().encounterTarget - d.save().explorationCredit : 0));
  d.check(d.screen().selected == (count ? 0u : unsigned(MenuItem::Region)),
          "menu focuses the waiting encounter, otherwise the first tile");
  d.check(count ? rendered_text::contains(menu, theme::kGold, 78, 130)
                : rendered_text::contains(walking, theme::kMuted, 78, 130),
          "menu banner appends the count directly, or shows the remaining walk");
  d.shot("queue-menu-" + std::to_string(count) + (english ? "-en" : ""));
  progression_support::toHome(d, "queue menu");
  if (count == 3 || english) {
    d.homeMenu(); d.select(MenuItem::Region); d.frames();
    d.shot(english ? "queue-region-hint-en" : "queue-region-hint-zh");
    progression_support::toHome(d, "queue region hint");
  }
}
void tapRect(Driver& d, const menu_layout::Rect& r) {
  d.tap(r.x + r.width / 2, r.y + r.height / 2);
}
void checkHomeShortcuts(Driver& d, bool english) {
  progression_support::toHome(d, "home shortcuts"); d.frames();
  tapRect(d, homeRegionTouchArea(d.runtime().language()));
  d.until([&] { return d.screen().page == Page::Routes; }, 4000, "home region pill opens routes directly");
  d.back();
  d.until([&] { return d.screen().page == Page::Home; }, 4000, "routes from home pill returns home");
  d.homeMenu(); d.select(MenuItem::Region);
  d.until([&] { return d.screen().page == Page::Routes; }, 4000, "menu region tile opens routes");
  d.back();
  d.until([&] { return d.screen().page == Page::Menu && d.screen().selected == unsigned(MenuItem::Region); },
          4000, "routes from menu returns to region tile");
  progression_support::toHome(d, "menu region return");
  if (pendingEncounterCount(d.save())) {
    tapRect(d, homeEncounterTouchArea(d.runtime().language(), d.save()));
    d.until([&] { return d.screen().page == Page::Encounter; }, 4000,
            english ? "English home encounter badge opens encounter directly"
                    : "Chinese home encounter badge opens encounter directly");
    d.select(3);
    d.until([&] { return d.screen().page == Page::Home; }, 4000, "battle later returns home");
  }
}
void walkToCount(Driver& d, unsigned count) {
  for (unsigned i = 0; pendingEncounterCount(d.save()) < count && i < count; ++i) {
    d.check(d.save().explorationCredit < d.save().encounterTarget, "walk begins below the next threshold");
    d.walk(unsigned(d.save().encounterTarget - d.save().explorationCredit));
  }
  d.check(pendingEncounterCount(d.save()) == count, "real walking builds the requested number of encounters");
}
}

void scenarioEncounterQueue(Driver& d, const std::string& phase) {
  d.frames();
  if (phase == "run") {
    bootNewSave(d); counts(d, 0);
    walkToCount(d, 1); counts(d, 1);
    walkToCount(d, 3); counts(d, 3); checkHomeShortcuts(d, false);
    walkToCount(d, 12); counts(d, 12);
    d.homeMenu(); d.select(MenuItem::Settings); d.select(1); d.select(1); d.select(2);
    counts(d, 12, true); checkHomeShortcuts(d, true);
    d.homeMenu(); d.select(MenuItem::Settings); d.select(1); d.select(0); d.select(2);
    progression_support::toHome(d, "Chinese queue");
    d.state("queue-end"); d.close();
  } else if (phase == "reload") {
    d.state("queue-restored"); counts(d, 12);
    d.check(d.save().encounter.id == 1, "new process restores the first queued encounter");
    d.homeMenu(); d.select(MenuItem::Encounter); d.select(0);
    d.check(d.screen().page == Page::Battle && pendingEncounterCount(d.save()) == 11,
            "starting the battle removes the active encounter from waiting count");
    d.winBattle();
    d.fault(Fault::BeforeWrite); d.throwBall(false);
    d.check(d.screen().page == Page::Error && d.save().encounter.usedBalls == 0 &&
            pendingEncounterCount(d.save()) == 11, "failed throw publishes neither attempt nor queue changes");
    d.fault(Fault::None); d.select(0);
    d.check(d.save().encounter.usedBalls == 1 && pendingEncounterCount(d.save()) == 11,
            "retry saves the same throw once without changing waiting encounters");
    d.throwBall(false); d.finishCapture();
    d.check(d.screen().page == Page::Result && d.save().result == Result::Escaped &&
            pendingEncounterCount(d.save()) == 11, "completed encounter is not counted as waiting");
    d.check(rendered_text::contains("下一场遭遇 (11)", theme::kGold), "result shows remaining queue CTA");
    d.press(SDL_SCANCODE_RETURN);
    d.until([&] { return d.screen().page == Page::Encounter; }, 4000,
            "confirming result with queued encounters enters next encounter directly");
    d.check(d.save().encounter.id == 2 && pendingEncounterCount(d.save()) == 11,
            "confirmation advances to the next FIFO encounter");
    d.select(3);
    d.until([&] { return d.screen().page == Page::Home; }, 4000, "battle later returns home");
    counts(d, 11); d.state("queue-end"); d.close();
  } else if (phase == "consumed-reload") {
    d.state("queue-restored"); counts(d, 11);
    d.check(d.save().encounter.id == 2, "restart does not replay a consumed encounter");
    walkToCount(d, 22); counts(d, 22); d.state("queue-end"); d.close();
  } else if (phase == "filled-reload") {
    d.state("queue-restored"); counts(d, 22);
    walkToCount(d, 32); d.walk(1600); counts(d, 32);
    const auto credit = d.save().explorationCredit, spent = d.save().explorationSpent;
    d.check(credit > d.save().encounterTarget, "full queue retains enough steps to reproduce immediate refill");
    d.homeMenu(); d.select(MenuItem::Encounter); d.select(0);
    d.winBattle(); d.throwBall(false); d.finishCapture(); d.throwBall(false); d.finishCapture();
    d.check(d.screen().page == Page::Result, "full-queue battle reaches its result");
    d.check(rendered_text::contains("下一场遭遇 (31)", theme::kGold), "full-queue result shows queue count");
    d.fault(Fault::BeforeWrite); d.press(SDL_SCANCODE_RETURN);
    d.check(d.screen().page == Page::Error && d.save().phase == Phase::Resolved,
            "failed result confirmation keeps the finished encounter available for retry");
    d.fault(Fault::None); d.select(0);
    d.check(d.save().explorationCredit == credit && d.save().explorationSpent == spent,
            "retry advances FIFO without consuming banked exploration steps");
    counts(d, 31); d.state("queue-end"); d.close();
  } else if (phase == "banked-reload") {
    d.state("queue-restored"); counts(d, 31);
    const auto credit = d.save().explorationCredit;
    const auto firstId = d.save().encounter.id;
    tapRect(d, homeEncounterTouchArea(d.runtime().language(), d.save()));
    d.until([&] { return d.screen().page == Page::Encounter; }, 4000,
            "home encounter badge starts encounter directly");
    d.select(0);
    d.winBattle(); d.throwBall(false); d.finishCapture(); d.throwBall(false); d.finishCapture();
    d.check(d.screen().page == Page::Result && rendered_text::contains("下一场遭遇 (30)", theme::kGold),
            "first queued result prompts next encounter 30");
    tapRect(d, menu_layout::rowBounds(0, menuTop(d.screen(), d.runtime().engine())));
    d.until([&] { return d.screen().page == Page::Encounter; }, 4000,
            "tapping result CTA chains directly into next queued encounter");
    d.check(d.save().encounter.id == firstId + 1 && pendingEncounterCount(d.save()) == 30,
            "chained encounter advances FIFO id");
    d.select(0);
    d.winBattle(); d.throwBall(false); d.finishCapture(); d.throwBall(false); d.finishCapture();
    d.check(d.screen().page == Page::Result && rendered_text::contains("下一场遭遇 (29)", theme::kGold),
            "second queued result prompts next encounter 29");
    acknowledge(d);
    counts(d, 29);
    d.check(d.save().explorationCredit == credit, "stationary battles continue draining the queue after restart");
    d.walk(1); counts(d, 32);
    d.check(d.save().explorationCredit < credit &&
            d.save().explorationCredit + d.save().explorationSpent == d.save().totalSteps,
            "new walking refills from retained credit without losing steps");
    d.state("queue-end"); d.close();
  } else if (phase == "refilled-reload") {
    d.state("queue-restored"); counts(d, 32);
    d.state("queue-end"); d.close();
  } else if (phase == "schema8") {
    d.check(d.save().schema == kSchemaVersion && d.save().encounter.id == 1 &&
            d.save().encounter.species == 72 && d.save().encounter.seed == 0x2468ace0u &&
            d.save().totalSteps == 300 && d.save().explorationCredit == 0,
            "schema8 single encounter migrates without changing its snapshot or step balance");
    counts(d, 1); walkToCount(d, 3); counts(d, 3);
    d.state("queue-end"); d.close();
  } else if (phase == "schema8-reload") {
    d.state("queue-restored"); counts(d, 3);
    d.check(d.save().encounter.id == 1 && d.save().encounter.species == 72,
            "migrated encounter stays first after adding queue entries and restarting");
    d.state("queue-end"); d.close();
  } else d.check(false, "unknown queue scenario phase");
}
} }
