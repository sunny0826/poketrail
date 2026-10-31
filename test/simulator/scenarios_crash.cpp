#include "driver.hpp"

namespace simulator {
namespace e2e {
namespace {
constexpr uint16_t kThrowSpecies = 72;
}

// Freeze at the recorded boundary until the parent sends SIGKILL.
void scenarioCrash(Driver& d, const std::string& phase) {
  if (phase == "paused") {
    d.frames();
    d.check(d.save().phase == Phase::EncounterReady, "battle fixture starts encounter ready");
    d.homeMenu();
    d.select(0);
    d.until([&] { return d.screen().page == Page::Encounter; }, 4000, "menu opens the encounter");
    d.select(0);
    d.until([&] { return d.screen().page == Page::Battle; }, 4000, "battle starts");
    d.tap(233, 35);
    d.until([&] { return d.screen().page == Page::Pause; }, 4000, "battle paused");
    d.shot("crash-paused");
    const uint64_t before = d.save().sequence;
    d.until([&] { return d.save().sequence > before; }, 90000, "pause checkpoint commits");
    d.check(d.save().sequence > before, "checkpoint committed while paused");
    d.state("crash-paused");
    d.event("kill-ready", "stage=paused sequence=" + std::to_string(d.save().sequence));
    d.freeze();
    return;
  }
  if (phase == "after-write" || phase == "partial-write") {
    d.frames();
    d.check(d.screen().page == Page::Throw, "throw fixture opens throw page");
    d.fault(phase == "after-write" ? Fault::AfterWrite : Fault::PartialWrite);
    d.throwBall(true);
    d.finishCapture();
    d.until([&] {
      const Page p = d.screen().page;
      return p == Page::Result || p == Page::Error || p == Page::Throw;
    }, 20000, "after-write failure settles");
    d.check(d.screen().page == Page::Error, "error shown after the write failure");
    d.check(d.faultHits() >= 1, "after-write fault recorded");
    d.state("crash-after-write");
    d.event("kill-ready", "stage=after-write");
    d.freeze();
    return;
  }
  if (phase == "reload-partial") {
    d.check(d.screen().page == Page::Throw && d.save().encounter.usedBalls == 0, "partial write falls back to pre-throw slot");
    d.check(!owns(d.save(), kThrowSpecies) && d.save().captureCounts[kThrowSpecies - 1] == 0 &&
            d.save().totalSteps == 800 && d.save().explorationCredit == 500, "partial-write recovery preserves exact accounts");
    d.state("partial-recovered"); d.close(); return;
  }
  if (phase == "reload-paused") {
    d.frames();
    d.check(d.save().phase == Phase::EncounterReady, "battle restores to encounter ready");
    d.check(d.screen().page == Page::Home, "home after restore");
    d.check(d.save().totalSteps == 800 && d.save().explorationCredit == 500 &&
            d.save().explorationSpent == 300 && d.save().encounter.id == 1 &&
            d.save().encounter.species == 72, "exact committed encounter and accounts recovered");
    d.check(d.save().result == Result::None, "no result resumed");
    d.shot("crash-reload-paused");
    d.close();
  }
  if (phase == "reload-capture") {
    d.frames();
    d.until([&] { return d.screen().page == Page::Result; }, 4000, "written capture restores to result");
    d.check(d.save().phase == Phase::Resolved, "restored capture waits for acknowledge");
    d.check(d.save().result == Result::Captured, "written capture survives the kill");
    d.check(d.save().captureCounts[kThrowSpecies - 1] == 1, "no duplicate award on restore");
    d.check(owns(d.save(), kThrowSpecies), "ownership restored from disk");
    d.shot("crash-reload-capture");
    d.press(SDL_SCANCODE_RETURN);
    d.until([&] { return d.screen().page == Page::Home; }, 4000, "acknowledged after restore");
    d.close();
  }
}

}  // namespace e2e
}  // namespace simulator
