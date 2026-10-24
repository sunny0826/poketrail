#include "battle.hpp"
namespace pokedex { namespace adventure {
bool timeAfter(uint32_t a, uint32_t b) { return int32_t(a - b) > 0; }
static uint32_t distance(uint32_t a, uint32_t b) {
  const uint32_t d = a - b;
  return d <= 0x7fffffffu ? d : 0u - d;
}
Timing battleTiming(uint32_t press, uint32_t target) {
  const uint32_t d = distance(press, target);
  return d <= 70 ? Timing::Perfect : d <= 180 ? Timing::Normal : Timing::Miss;
}
Timing throwTiming(uint32_t press, uint32_t target) {
  const uint32_t d = distance(press, target);
  return d <= 70 ? Timing::Perfect : d <= 120 ? Timing::Nice :
         d <= 180 ? Timing::Normal : Timing::Miss;
}
void Battle::begin(uint32_t now, uint32_t seed) {
  view_ = BattleView();
  view_.active = true;
  view_.targetMs = now + kBeatIntervalMs + seed % 301;
  view_.settledMs = now - kBeatIntervalMs;
}
void Battle::settle(Timing timing, uint32_t now) {
  view_.settledMs = now;
  view_.consumed = true;
  view_.lastTiming = timing;
  if (view_.expected == Action::Attack && timing != Timing::Miss) {
    const uint8_t damage = timing == Timing::Perfect ? 2 : 1;
    view_.enemyHp = view_.enemyHp > damage ? view_.enemyHp - damage : 0;
  } else if (view_.expected == Action::Dodge && timing == Timing::Miss) {
    --view_.playerHp;
  }
  if (!view_.enemyHp || !view_.playerHp) {
    view_.active = false;
    view_.result = !view_.enemyHp ? Result::Won : Result::Lost;
  }
}
void Battle::advance() {
  ++view_.cueIndex;
  if (view_.cueIndex == 12) {
    view_.active = false;
    view_.result = Result::Lost;
    return;
  }
  view_.expected = view_.cueIndex % 2 ? Action::Dodge : Action::Attack;
  view_.consumed = false;
  view_.targetMs += kBeatIntervalMs;
  if (view_.pauseRequested) { view_.paused = true; view_.pauseRequested = false; }
}
bool Battle::tick(uint32_t now) {
  bool changed = false;
  while (view_.active && !view_.paused && timeAfter(now, view_.targetMs + 180)) {
    if (!view_.consumed) settle(Timing::Miss, now);
    if (view_.active) advance();
    changed = true;
  }
  return changed;
}
bool Battle::press(Action action, uint32_t now) {
  const uint8_t previous = view_.cueIndex;
  tick(now);
  if (!view_.active || view_.paused || view_.consumed || previous != view_.cueIndex) return false;
  settle(action == view_.expected ? battleTiming(now, view_.targetMs) : Timing::Miss, now);
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
  view_.targetMs = now + kBeatIntervalMs;
  view_.settledMs = now - kBeatIntervalMs;
  return true;
}
} }
