#include "battle.hpp"
namespace pokedex { namespace adventure {
bool timeAfter(uint32_t a, uint32_t b) { return int32_t(a - b) > 0; }
static uint32_t distance(uint32_t a, uint32_t b) {
  const uint32_t d = a - b;
  return d <= 0x7fffffffu ? d : 0u - d;
}
static Action opposite(Action a) { return a == Action::Attack ? Action::Dodge : Action::Attack; }
static uint32_t normalWindow(bool frenzy) { return frenzy ? kFrenzyNormalWindowMs : kNormalWindowMs; }
Timing battleTiming(uint32_t press, uint32_t target, bool frenzy) {
  const uint32_t d = distance(press, target);
  return d <= (frenzy ? kFrenzyPerfectWindowMs : kPerfectWindowMs) ? Timing::Perfect :
         d <= normalWindow(frenzy) ? Timing::Normal : Timing::Miss;
}
Timing throwTiming(uint32_t press, uint32_t target) {
  const uint32_t d = distance(press, target);
  return d <= 70 ? Timing::Perfect : d <= 120 ? Timing::Nice :
         d <= 180 ? Timing::Normal : Timing::Miss;
}
Action shownAction(const BattleView& v, uint32_t now) {
  if (!v.feint || !timeAfter(v.targetMs - kFeintFlipMs, now)) return v.expected;
  return opposite(v.expected);
}
Action shownNextAction(const BattleView& v) { return v.nextFeint ? opposite(v.nextExpected) : v.nextExpected; }
uint8_t countdownDigit(const BattleView& v, uint32_t now) {
  const uint32_t cue = v.targetMs - kCueLeadMs;
  if (!v.active || v.paused || !v.opening || !timeAfter(cue, now)) return 0;
  return uint8_t((cue - now + kCountdownStepMs - 1) / kCountdownStepMs);
}
namespace {
struct Cue { bool attack, feint; uint8_t history; };
Cue decide(uint32_t seed, uint8_t index, uint8_t history, uint8_t feintPercent) {
  const uint32_t h = mixRandom(seed ^ (uint32_t(index) * 0x9e3779b9u) ^ 0x5bd1e995u);
  // Two equal actions in a row force a switch, so every seed offers at least 5 attack cues.
  const uint8_t last = history & 3;
  const bool attack = index >= 2 && last == 0 ? true : index >= 2 && last == 3 ? false : h % 100 < 55;
  return {attack, index > 0 && (h >> 8) % 100 < feintPercent, uint8_t((history << 1 | (attack ? 1 : 0)) & 3)};
}
}
uint32_t Battle::interval() const { return view_.frenzy ? kFrenzyBeatIntervalMs : kBeatIntervalMs; }
void Battle::plan() {
  const Cue cue = decide(seed_, view_.cueIndex, history_, feintPercent_);
  view_.expected = cue.attack ? Action::Attack : Action::Dodge;
  view_.feint = cue.feint;
  history_ = cue.history;
  const Cue next = decide(seed_, uint8_t(view_.cueIndex + 1), history_, feintPercent_);
  view_.nextExpected = next.attack ? Action::Attack : Action::Dodge;
  view_.nextFeint = next.feint;
}
void Battle::begin(uint32_t now, uint32_t seed, bool rare) {
  view_ = BattleView();
  seed_ = seed; history_ = 0;
  feintPercent_ = rare ? kRareFeintPercent : kFeintPercent;
  view_.active = true;
  view_.opening = true;
  plan();
  view_.targetMs = now + kOpeningMs;
  view_.settledMs = now - kBeatIntervalMs;
}
void Battle::settle(Action action, Timing timing, uint32_t now) {
  ++view_.settlements;
  view_.settledMs = now;
  view_.consumed = true;
  view_.settledAction = action;
  view_.settledCue = view_.expected;
  view_.special = false;
  if (action == Action::Both && view_.energy >= kSpecialEnergy) {
    view_.energy = 0;
    if (view_.expected == Action::Attack && timing != Timing::Miss) {
      view_.special = true; view_.lastTiming = timing;
      view_.enemyHp = view_.enemyHp > kSpecialDamage ? view_.enemyHp - kSpecialDamage : 0;
    } else {
      view_.lastTiming = Timing::Miss; --view_.playerHp;
    }
  } else {
    if (action != view_.expected) timing = Timing::Miss;
    view_.lastTiming = timing;
    if (view_.expected == Action::Attack && timing != Timing::Miss) {
      const uint8_t damage = timing == Timing::Perfect ? 2 : 1;
      view_.enemyHp = view_.enemyHp > damage ? view_.enemyHp - damage : 0;
    } else if (view_.expected == Action::Dodge && timing == Timing::Miss) {
      --view_.playerHp;
    }
    if (timing == Timing::Perfect && view_.energy < kSpecialEnergy) ++view_.energy;
  }
  if (!view_.enemyHp || !view_.playerHp) {
    view_.active = false;
    view_.result = !view_.enemyHp ? Result::Won : Result::Lost;
  }
}
bool Battle::upgrade(Action action, uint32_t now) {
  const Action first = view_.settledAction;
  if (first == Action::Both || beforeEnergy_ < kSpecialEnergy || timeAfter(now, pressMs_ + kChordGraceMs) ||
      (action != Action::Both && action == first)) return false;
  view_.playerHp = beforePlayerHp_; view_.enemyHp = beforeEnemyHp_; view_.energy = beforeEnergy_;
  settle(Action::Both, battleTiming(pressMs_, view_.targetMs, view_.frenzy), now);
  return true;
}
void Battle::advance() {
  ++view_.cueIndex;
  if (view_.cueIndex == kMaxCues) {
    view_.active = false;
    view_.result = Result::Lost;
    return;
  }
  view_.frenzy = view_.enemyHp * 2 < kEnemyHp;
  view_.opening = false;
  view_.consumed = false;
  view_.special = false;
  plan();
  view_.targetMs += interval();
  if (view_.pauseRequested) { view_.paused = true; view_.pauseRequested = false; }
}
bool Battle::tick(uint32_t now) {
  bool changed = false;
  while (view_.active && !view_.paused && timeAfter(now, view_.targetMs + normalWindow(view_.frenzy))) {
    if (!view_.consumed) settle(view_.expected, Timing::Miss, now);
    if (view_.active) advance();
    changed = true;
  }
  return changed;
}
bool Battle::press(Action action, uint32_t now) {
  const uint8_t previous = view_.cueIndex;
  tick(now);
  if (!view_.active || view_.paused || previous != view_.cueIndex || countdownDigit(view_, now)) return false;
  if (view_.consumed) return upgrade(action, now);
  pressMs_ = now;
  beforePlayerHp_ = view_.playerHp; beforeEnemyHp_ = view_.enemyHp; beforeEnergy_ = view_.energy;
  settle(action, battleTiming(now, view_.targetMs, view_.frenzy), now);
  return true;
}
bool Battle::requestPause() {
  if (!view_.active || view_.paused) return false;
  view_.pauseRequested = true;
  return true;
}
bool Battle::resume(uint32_t now) {
  if (!view_.active || !view_.paused) return false;
  view_.paused = false;
  view_.opening = true;
  view_.targetMs = now + kOpeningMs;
  view_.settledMs = now - interval();
  return true;
}
} }
