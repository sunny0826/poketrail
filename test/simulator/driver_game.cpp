#include "driver.hpp"
#include "battle_presentation.hpp"

namespace simulator { namespace e2e {
void Driver::winBattle(bool practice) {
  const auto view = [&]() -> const BattleView& { return practice ? runtime().practiceView() : runtime().engine().battleView(); };
  check(screen().page == (practice ? Page::Practice : Page::Battle), "battle page visible");
  const auto start = now(); unsigned lastCue = 999;
  while (view().active && now() - start < 40000) {
    const auto v = view();
    if (!v.consumed && v.cueIndex != lastCue && int32_t(v.targetMs - now()) <= 60) {
      check(!v.paused && !battleEffectActive(v, now()), "battle cue visible before input");
      const auto action = shownAction(v, now());
      event("visible-cue", std::to_string(v.cueIndex) + ":" + std::to_string(int(action)));
      if (lastCue == 999) shot(practice ? "practice-cue" : "battle-cue");
      lastCue = v.cueIndex;
      press(action == Action::Attack ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT);
    } else frames(1);
  }
  check(view().result == Result::Won, "visible cue inputs win battle");
  until([&] { return screen().page == (practice ? Page::PracticeThrow : Page::Throw); }, 2000, "win reaches throw page");
}
void Driver::replayBattle() {
  // Offsets and directions frozen from the recorded #590 encounter trace.
  const uint32_t start = now();
  const uint32_t offsets[] = {1400, 2900, 4000, 5100, 6200};
  const SDL_Scancode keys[] = {SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                             SDL_SCANCODE_LEFT, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT};
  check(screen().page == Page::Battle, "fixed replay starts on battle page");
  for (unsigned i = 0; i < 5; ++i) {
    advance(offsets[i] - (now() - start));
    check(screen().page == Page::Battle, "fixed replay remains in battle");
    shot("fixed-cue-" + std::to_string(i));
    press(keys[i]);
  }
  check(runtime().engine().battleView().result == Result::Won, "fixed input trace wins without reading cues");
  until([&] { return screen().page == Page::Throw; }, 2000, "fixed replay reaches throw page");
}
void Driver::throwBall(bool perfect, bool practice) {
  check(screen().page == (practice ? Page::PracticeThrow : Page::Throw), "throw page visible");
  frames();
  const auto active = [&] { return practice ? screen().practiceThrowing : runtime().engine().throwView().active; };
  check(!active(), "the throw window is closed before the first hold");
  // The window opens on the first hold, so a tap must not start it.
  tap(233, 238);
  check(!active(), "a tap before the hold does not open the throw window");
  key(SDL_SCANCODE_RIGHT, true);
  check(active(), "holding right opens the throw window");
  check(screen().charging, "holding right charges ball");
  const auto target = practice ? screen().practiceTargetMs : runtime().engine().throwView().targetMs;
  if (perfect) until([&] { return int32_t(target - now()) <= 20; }, 3000, "charge reaches release target");
  // A miss still holds past the minimum hold, so it counts as one attempt.
  else advance(400);
  key(SDL_SCANCODE_RIGHT, false, 1);
  check(!active(), "release settles throw");
  state("throw-settled");
}
void Driver::finishCapture() {
  until([&] { return !screen().capture.active; }, 20000, "capture animation completes"); frames();
}
} }
