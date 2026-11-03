#include "driver.hpp"
#include "battle_presentation.hpp"
#include "../support/animation_feedback.hpp"
#include "../support/capture_badge.hpp"

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
      if (lastCue == 999) {
        shot(practice ? "practice-cue" : "battle-cue");
        bool full = true;
        for (unsigned i = 0; i < kPlayerHp; ++i)
          full = full && M5.Display.readPixel(hpSegmentX(false, i, kPlayerHp), kHpSegmentY + 3) == theme::kMint;
        for (unsigned i = 0; i < kEnemyHp; ++i)
          full = full && M5.Display.readPixel(hpSegmentX(true, i, kEnemyHp), kHpSegmentY + 3) == theme::kGold;
        check(full, "battle HUD plates show both full HP tracks at the first cue");
      }
      lastCue = v.cueIndex;
      press(action == Action::Attack ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT);
      const auto& settled = view();
      const bool hit = settled.lastTiming != Timing::Miss;
      const bool english = runtime().language() == pokedex::i18n::Language::English;
      const char* feedback = english ?
          (action == Action::Attack ? (hit ? "Hit" : "Miss") : (hit ? "Dodged" : "Hurt")) :
          (action == Action::Attack ? (hit ? "命中" : "未命中") : (hit ? "躲开了" : "受伤了"));
      const uint16_t color = settled.lastTiming == Timing::Perfect ? theme::kGold : hit ? theme::kText : theme::kMuted;
      check(animation_feedback_test::label(feedback, color, 401), "battle pixels identify the settled action result");
      if (v.cueIndex == 0) shot(practice ? "practice-action-feedback" : "battle-action-feedback");
    } else frames(1);
  }
  check(view().result == Result::Won, "visible cue inputs win battle");
  until([&] { return screen().page == (practice ? Page::PracticeThrow : Page::Throw); }, 2000, "win reaches throw page");
}
void Driver::replayBattle() {
  // Offsets and directions frozen from the recorded seed-1745833800 encounter trace; each press lands
  // about 100ms before its beat, the first beat being kOpeningMs after the battle starts.
  const uint32_t start = now();
  const uint32_t offsets[] = {2012, 3512, 4612, 5712, 6812};
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
  const auto arc = [](int dx) { return M5.Display.readPixel(233 + dx, kHandY) == theme::kGold; };
  if (perfect) {
    until([&] { return int32_t(target - now()) <= 20; }, 3000, "charge reaches release target");
    check(arc(29) && arc(-29), "full charge closes the gold arc around the held ball");
  } else {
    // A miss still holds past the minimum hold, so it counts as one attempt.
    advance(400);
    check(arc(29) && !arc(-29), "half charge fills the arc clockwise past 3 o'clock only");
  }
  key(SDL_SCANCODE_RIGHT, false, 1);
  check(!active(), "release settles throw");
  state("throw-settled");
}
void Driver::finishCapture() {
  if (screen().capture.active) {
    const auto anim = screen().capture;
    const auto committed = save();
    const auto holdAt = anim.startMs + captureTotalMs(anim) - (anim.captured ? 1000 : 400) + 100;
    until([&] { return int32_t(now() - holdAt) >= 0; }, 10000, "capture reaches its result hold");
    check(screen().capture.active && captureFrameAt(screen().capture, now()).stage == CaptureStage::Reveal,
          "capture result remains on screen during its hold");
    const bool english = runtime().language() == pokedex::i18n::Language::English;
    const char* outcome = english ? (anim.captured ? "Caught!" : "Broke free") :
                                    (anim.captured ? "捕获成功" : "挣脱了");
    check(animation_feedback_test::label(outcome, anim.captured ? theme::kGold : theme::kText),
          "capture outcome is legible in actual rendered pixels");
    if (anim.captured) check(capture_badge_test::iconAt(kBallX, kGroundY), "successful ball remains visible through the hold");
    check(save().encounter.id == committed.encounter.id &&
          save().encounter.usedBalls == committed.encounter.usedBalls &&
          save().phase == committed.phase && save().result == committed.result &&
          save().ballsSpent == committed.ballsSpent && save().rosterRevision == committed.rosterRevision &&
          save().owned == committed.owned && save().registered == committed.registered &&
          save().captureCounts == committed.captureCounts && save().recentSize == committed.recentSize,
          "animation preserves attempts and rewards across background checkpoints");
    shot(anim.captured ? "capture-success-hold" : "capture-breakout-hold");
  }
  until([&] { return !screen().capture.active; }, 20000, "capture animation completes"); frames();
}
} }
