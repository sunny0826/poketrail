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
uint32_t Battle::interval() const { return view_.frenzy ? kFrenzyBeatIntervalMs : kBeatIntervalMs; }
void Battle::plan() {
  const uint32_t h = mixRandom(seed_ ^ (uint32_t(view_.cueIndex) * 0x9e3779b9u) ^ 0x5bd1e995u);
  // Two equal actions in a row force a switch, so every seed offers at least 5 attack cues.
  const uint8_t last = history_ & 3;
  const bool attack = view_.cueIndex >= 2 && last == 0 ? true :
                      view_.cueIndex >= 2 && last == 3 ? false : h % 100 < 55;
  view_.expected = attack ? Action::Attack : Action::Dodge;
  history_ = uint8_t((history_ << 1 | (attack ? 1 : 0)) & 3);
  view_.feint = view_.cueIndex > 0 && (h >> 8) % 100 < feintPercent_;
}
void Battle::begin(uint32_t now, uint32_t seed, bool rare) {
  view_ = BattleView();
  seed_ = seed; history_ = 0;
  feintPercent_ = rare ? kRareFeintPercent : kFeintPercent;
  view_.active = true;
  plan();
  view_.targetMs = now + kBeatIntervalMs + seed % 301;
  view_.settledMs = now - kBeatIntervalMs;
}
void Battle::settle(Action action, Timing timing, uint32_t now) {
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
  if (!view_.active || view_.paused || previous != view_.cueIndex) return false;
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
  view_.targetMs = now + interval();
  view_.settledMs = now - interval();
  return true;
}
} }
