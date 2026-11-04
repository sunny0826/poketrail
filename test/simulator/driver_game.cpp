#include "driver.hpp"
#include "battle_presentation.hpp"
#include "../support/animation_feedback.hpp"

namespace simulator { namespace e2e {
void Driver::winBattle(bool practice) {
  const auto view = [&]() -> const BattleView& { return practice ? runtime().practiceView() : runtime().engine().battleView(); };
  check(screen().page == (practice ? Page::Practice : Page::Battle), "battle page visible");
  const auto start = now(); unsigned lastCue = 999;
  while (view().active && now() - start < 40000) {
    const auto v = view();
    if (!v.consumed && v.cueIndex != lastCue && int32_t(v.targetMs - now()) <= 60) {
      const auto action = shownAction(v, now());
      event("visible-cue", std::to_string(v.cueIndex) + ":" + std::to_string(int(action)));
      lastCue = v.cueIndex;
      press(action == Action::Attack ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT);
    } else frames(1);
  }
  check(view().result == Result::Won, "cue inputs win battle");
  until([&] { return screen().page == (practice ? Page::PracticeThrow : Page::Throw); }, 2000, "win reaches throw page");
}
void Driver::replayBattle() {
  // Recorded player inputs are independent of Runtime's cue selection.
  const uint32_t start = now();
  const uint32_t offsets[] = {2012, 3512, 4612, 5712, 6812};
  const SDL_Scancode keys[] = {SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                             SDL_SCANCODE_LEFT, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT};
  check(screen().page == Page::Battle, "fixed replay starts on battle page");
  for (unsigned i = 0; i < 5; ++i) {
    until([&] { return now() - start >= offsets[i]; }, 3000, "recorded input time reached");
    press(keys[i]);
  }
  check(runtime().engine().battleView().result == Result::Won, "recorded player inputs win without reading cues");
  until([&] { return screen().page == Page::Throw; }, 2000, "fixed replay reaches throw page");
}
void Driver::throwBall(bool perfect, bool practice) {
  check(screen().page == (practice ? Page::PracticeThrow : Page::Throw), "throw page visible");
  frames();
  key(SDL_SCANCODE_RIGHT, true);
  check(screen().charging, "holding right charges ball");
  const auto target = practice ? screen().practiceTargetMs : runtime().engine().throwView().targetMs;
  if (perfect) {
    until([&] { return int32_t(target - now()) <= 20; }, 3000, "hold reaches release target");
  } else {
    advance(400);
  }
  key(SDL_SCANCODE_RIGHT, false, 1);
}
void Driver::finishCapture() {
  if (screen().capture.active) {
    const auto anim = screen().capture;
    const auto committed = save();
    const auto holdAt = anim.startMs + captureTotalMs(anim) - (anim.captured ? 1000 : 400) + 100;
    until([&] { return int32_t(now() - holdAt) >= 0; }, 10000, "capture reaches its result hold");
    const bool english = runtime().language() == pokedex::i18n::Language::English;
    const char* outcome = english ? (anim.captured ? "Caught!" : "Broke free") :
                                    (anim.captured ? "捕获成功" : "挣脱了");
    check(animation_feedback_test::label(outcome, anim.captured ? theme::kGold : theme::kText),
          "capture outcome is legible in actual rendered pixels");
    check(save().encounter.usedBalls == committed.encounter.usedBalls &&
          save().ballsSpent == committed.ballsSpent && save().owned == committed.owned &&
          save().captureCounts == committed.captureCounts,
          "animation does not spend attempts or award captures again");
  }
  until([&] { return !screen().capture.active; }, 20000, "capture animation completes"); frames();
}
} }
