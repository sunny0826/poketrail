#include "driver.hpp"
#include "battle_presentation.hpp"
#include "../support/battle_cue_checks.hpp"
#include "../support/capture_badge.hpp"

namespace simulator { namespace e2e {
namespace {
using namespace battle_cue_test;
const BattleView& battle(Driver& d) { return d.runtime().engine().battleView(); }
uint16_t expectedBuzz(const BattleView& b) {
  return b.special || b.lastTiming == Timing::Perfect ? 40 : b.lastTiming == Timing::Miss ? 10 : 20;
}
bool ballSlot(unsigned index) { return capture_badge_test::iconAt(ballSlotX(index), kBallSlotY); }
bool emptySlot(unsigned index) {
  return !ballSlot(index) && M5.Display.readPixel(ballSlotX(index) + 13, kBallSlotY) == theme::kDisabled;
}
void countdown(Driver& d, const std::string& name) {
  d.frames(2);
  d.check(countdownDigit(battle(d), d.now()) == 3 && countdownDigitShown(3) && noCue(),
          name + " opens on 3 with no cue drawn");
  d.check(pauseGlyph(), name + " shows the pause glyph instead of a caption");
  d.shot(name + "-countdown-3");
  const auto before = battle(d);
  const unsigned buzzes = d.feedbacks();
  d.press(SDL_SCANCODE_LEFT); d.press(SDL_SCANCODE_RIGHT);
  d.check(!battle(d).consumed && battle(d).settlements == before.settlements &&
          battle(d).playerHp == before.playerHp && battle(d).enemyHp == before.enemyHp && d.feedbacks() == buzzes,
          name + " ignores both keys during the countdown");
  d.until([&] { return countdownDigit(battle(d), d.now()) == 1; }, 2000, name + " countdown reaches 1");
  d.frames(2);
  d.check(countdownDigitShown(1), name + " shows 1 before the first cue");
}
bool cueVisibleOn(Action action, uint16_t& color, bool frenzy) {
  for (uint16_t candidate : {action == Action::Attack ? theme::kRed : theme::kBlue, theme::kText}) {
    if (candidate == theme::kText && !frenzy) break;
    if (cueOn(action, candidate)) { color = candidate; return true; }
  }
  return false;
}
// Reads each cue from the screen ~400ms ahead, then plays it on the beat with the key it points to.
void playCues(Driver& d) {
  unsigned lastCue = 999; bool paused = false, sawFeint = false, usedSpecial = false;
  const uint32_t start = d.now();
  while (battle(d).active && d.now() - start < 60000) {
    const auto v = battle(d);
    const int32_t remaining = int32_t(v.targetMs - d.now());
    if (v.paused || v.consumed || v.cueIndex == lastCue || remaining > 400) { d.frames(1); continue; }
    lastCue = v.cueIndex;
    const Action shown = shownAction(v, d.now());
    uint16_t color = 0;
    d.check(cueVisibleOn(shown, color, v.frenzy), "cue " + std::to_string(v.cueIndex) + " circles the actor of its key");
    d.check(approachRadius(shown, color) > kCueTargetRadius + 2, "approach ring is still closing ahead of the beat");
    d.check(nextOnlyLane(v, d.now()), "lane has one neutral next-beat dot, no arrows, direction colours, timing box or ticks");
    d.check(noActionPrompt(), "current beat has no duplicate attack, dodge or special caption");
    d.event("visible-cue", std::to_string(v.cueIndex) + ":" + std::to_string(int(shown)) + (v.feint ? ":feint" : ""));
    if (v.cueIndex == 0) d.shot("battle-cues-first");
    if (v.feint) {
      sawFeint = true;
      d.shot("battle-cues-feint-" + std::to_string(v.cueIndex));
      d.until([&] { return int32_t(battle(d).targetMs - d.now()) <= 260; }, 1000, "feint switches sides");
      const Action revealed = shownAction(battle(d), d.now());
      d.check(revealed != shown && cueVisibleOn(revealed, color, v.frenzy), "feint moves only the current ring to the judged side");
      d.check(nextOnlyLane(battle(d), d.now()), "next preview stays neutral through the current feint");
      d.shot("battle-cues-revealed-" + std::to_string(v.cueIndex));
    }
    d.until([&] { return int32_t(battle(d).targetMs - d.now()) <= 60; }, 1000, "beat approaches");
    d.check(nextOnlyLane(battle(d), d.now()) && noActionPrompt(), "perfect window adds no lane target or action caption");
    const Action key = shownAction(battle(d), d.now());
    const bool special = v.frenzy && key == Action::Attack && battle(d).energy == kSpecialEnergy;
    const unsigned buzzes = d.feedbacks();
    d.press(special ? SDL_SCANCODE_RETURN : key == Action::Attack ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT);
    if (special) {
      usedSpecial = true;
      d.check(battle(d).special && battle(d).energy == 0, "SDL two-key chord still releases a charged special without an action caption");
      d.shot("battle-cues-special-result");
    }
    d.check(battle(d).settlements == v.settlements + 1 && battle(d).lastTiming != Timing::Miss,
            "the key the cue pointed to is judged on the beat");
    d.check(d.feedbacks() == buzzes + 1 && d.lastFeedback() == expectedBuzz(battle(d)),
            "each judged beat vibrates once with its timing strength");
    if (!paused && battle(d).active) {
      paused = true;
      d.tap(233, 30);
      d.until([&] { return d.screen().page == Page::Pause; }, 3000, "top tap pauses after the beat");
      d.shot("battle-cues-paused");
      d.select(0);
      d.until([&] { return d.screen().page == Page::Battle; }, 2000, "resume returns to battle");
      countdown(d, "battle-cues-resume");
    }
  }
  d.check(paused && sawFeint && usedSpecial && battle(d).result == Result::Won,
          "reading only the rings wins with a feint, special and pause/resume");
}
}

// P0-02：收圈独占当前拍、轨道仅中性下一拍圆点且无箭头/判定框/刻度/动作文案、开场与恢复倒计时、
// 判定振动；投球页标题下的两个球位随消耗变空并在新进程恢复。
void scenarioBattle(Driver& d, const std::string& phase) {
  if (phase == "cues") {
    d.frames();
    d.check(d.save().phase == Phase::EncounterReady, "battle fixture starts encounter ready");
    d.homeMenu();
    d.select(0);
    d.until([&] { return d.screen().page == Page::Encounter; }, 4000, "menu opens the encounter");
    d.select(0);
    d.until([&] { return d.screen().page == Page::Battle; }, 4000, "battle starts");
    countdown(d, "battle-cues");
    playCues(d);
    d.until([&] { return d.screen().page == Page::Throw; }, 2000, "victory reaches the throw page");
    d.frames(3);
    d.check(ballSlot(0) && ballSlot(1), "throw title shows both attempts as balls");
    d.shot("battle-cues-throw-ready");
    d.throwBall(false);
    d.finishCapture();
    d.until([&] { return d.screen().page == Page::Throw || d.screen().page == Page::Result; }, 20000, "miss settles");
    d.check(d.screen().page == Page::Throw && d.save().encounter.usedBalls == 1, "one miss keeps one attempt");
    d.frames(3);
    d.check(emptySlot(0) && ballSlot(1), "the used attempt turns into an empty ring");
    d.shot("battle-cues-throw-one-left");
    d.close();
  } else if (phase == "reload") {
    d.frames(3);
    d.check(d.screen().page == Page::Throw && d.save().phase == Phase::ThrowReady &&
            d.save().encounter.usedBalls == 1 && !d.runtime().engine().throwView().active,
            "restart restores the throw page with one attempt left");
    d.check(emptySlot(0) && ballSlot(1), "restored throw title still shows one empty and one ball");
    d.shot("battle-cues-reload");
    d.close();
  }
}
} }
