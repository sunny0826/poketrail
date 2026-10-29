#include "battle.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace pokedex::adventure;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace {
struct Cue { Action expected; bool feint; uint32_t targetMs; bool frenzy; };
// Plays every cue with `pick`, records the sequence the player actually faced.
template <typename Pick>
std::vector<Cue> play(Battle& b, uint32_t seed, Pick pick, bool rare = false) {
  std::vector<Cue> cues; b.begin(1000, seed, rare);
  while (b.view().active) {
    const BattleView v = b.view();
    cues.push_back({v.expected, v.feint, v.targetMs, v.frenzy});
    pick(b, v);
    b.tick(v.targetMs + kNormalWindowMs + 1);
  }
  return cues;
}
void perfect(Battle& b, const BattleView& v) { b.press(v.expected, v.targetMs); }
void idle(Battle&, const BattleView&) {}
// Perfect dodges build energy while normal attacks keep the enemy alive.
void charge(Battle& b, const BattleView& v) { b.press(v.expected, v.targetMs + (v.expected == Action::Attack ? 100 : 0)); }
void chargeUntil(Battle& b, Action next) {
  while (b.view().active && (b.view().energy < kSpecialEnergy || b.view().expected != next)) {
    charge(b, b.view()); b.tick(b.view().targetMs + 181);
  }
}

void sameSeedSameCues() {
  Battle a, b, c;
  const auto x = play(a, 42, idle), y = play(b, 42, idle);
  CHECK(x.size() == y.size());
  for (size_t i = 0; i < x.size(); ++i)
    CHECK(x[i].expected == y[i].expected && x[i].feint == y[i].feint && x[i].targetMs == y[i].targetMs);
  bool differs = false;
  for (uint32_t seed = 1; seed < 20 && !differs; ++seed) {
    const auto z = play(c, seed, idle);
    for (size_t i = 0; i < std::min(x.size(), z.size()); ++i) differs |= x[i].expected != z[i].expected;
  }
  CHECK(differs);
  std::puts("PASS same seed replays the same cues; other seeds differ");
}

void cuesAreNotStrictlyAlternating() {
  unsigned repeats = 0, feints = 0, rareFeints = 0, total = 0;
  for (uint32_t seed = 0; seed < 200; ++seed) {
    Battle b, r;
    const auto cues = play(b, seed, idle), rare = play(r, seed, idle, true);
    for (size_t i = 1; i < cues.size(); ++i) repeats += cues[i].expected == cues[i - 1].expected;
    for (size_t i = 0; i < cues.size(); ++i) { feints += cues[i].feint; ++total; }
    for (const Cue& c : rare) rareFeints += c.feint;
    CHECK(!cues[0].feint);
    for (size_t i = 2; i < cues.size(); ++i)
      CHECK(!(cues[i].expected == cues[i - 1].expected && cues[i - 1].expected == cues[i - 2].expected));
  }
  CHECK(repeats > 0 && feints > 0 && rareFeints > feints);
  std::printf("PASS cues repeat %u times, feints %u/%u (rare %u), never three in a row\n", repeats, feints, total, rareFeints);
}

void feintFlipsLate() {
  BattleView v; v.expected = Action::Attack; v.feint = true; v.targetMs = 10000;
  CHECK(shownAction(v, 10000 - kCueLeadMs) == Action::Dodge);
  CHECK(shownAction(v, 10000 - kFeintFlipMs - 1) == Action::Dodge);
  CHECK(shownAction(v, 10000 - kFeintFlipMs) == Action::Attack);
  v.feint = false; CHECK(shownAction(v, 10000 - kCueLeadMs) == Action::Attack);
  // Judging follows the flipped action, never the early decoy.
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle b; b.begin(1000, seed);
    while (b.view().active && !b.view().feint) { perfect(b, b.view()); b.tick(b.view().targetMs + 181); }
    if (!b.view().active) continue;
    const BattleView f = b.view(); const uint8_t hp = f.playerHp;
    CHECK(b.press(shownAction(f, f.targetMs - kCueLeadMs), f.targetMs));
    CHECK(b.view().lastTiming == Timing::Miss);
    CHECK(f.expected == Action::Attack || b.view().playerHp == hp - 1);
    std::puts("PASS feint shows the decoy until 350ms before the beat and judges the real action");
    return;
  }
  CHECK(false);
}

void frenzyAfterHalfHp() {
  Battle b; b.begin(0, 7);
  CHECK(b.view().enemyHp == kEnemyHp && b.view().playerHp == kPlayerHp && !b.view().frenzy);
  CHECK(battleTiming(1180, 1000) == Timing::Normal && battleTiming(1181, 1000) == Timing::Miss);
  CHECK(battleTiming(930, 1000) == Timing::Perfect && battleTiming(929, 1000) == Timing::Normal);
  CHECK(battleTiming(1140, 1000, true) == Timing::Normal && battleTiming(1141, 1000, true) == Timing::Miss);
  CHECK(battleTiming(1055, 1000, true) == Timing::Perfect && battleTiming(1056, 1000, true) == Timing::Normal);
  bool entered = false;
  while (b.view().active) {
    const BattleView v = b.view();
    CHECK(v.frenzy == (v.enemyHp * 2 < kEnemyHp));
    if (v.frenzy && v.expected == Action::Attack && !entered) {
      entered = true;
      CHECK(!b.press(Action::Attack, v.targetMs + kFrenzyNormalWindowMs + 1));
      CHECK(b.view().lastTiming == Timing::Miss && b.view().enemyHp == v.enemyHp);
      CHECK(!b.view().active || b.view().targetMs == v.targetMs + kFrenzyBeatIntervalMs);
      continue;
    }
    perfect(b, v); b.tick(v.targetMs + kNormalWindowMs + 1);
  }
  CHECK(entered && b.view().result == Result::Won);
  std::puts("PASS frenzy below half HP tightens windows to 140/55ms and beats to 1100ms");
}

void specialMove() {
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle b; b.begin(0, seed);
    // Both keys without energy is a plain miss and earns nothing.
    BattleView v = b.view();
    CHECK(b.press(Action::Both, v.targetMs) && b.view().lastTiming == Timing::Miss && !b.view().energy);
    b.tick(v.targetMs + 181);
    chargeUntil(b, Action::Attack);
    if (!b.view().active || b.view().enemyHp <= kSpecialDamage) continue;
    v = b.view();
    CHECK(b.press(Action::Both, v.targetMs + 100));
    CHECK(b.view().special && b.view().energy == 0 && b.view().enemyHp == v.enemyHp - kSpecialDamage);
    std::puts("PASS full energy chord on an attack cue deals 3 and drains energy");
    return;
  }
  CHECK(false);
}

void specialMisfire() {
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle b; b.begin(0, seed);
    chargeUntil(b, Action::Dodge);
    if (!b.view().active || b.view().energy < kSpecialEnergy || b.view().playerHp < 2) continue;
    const BattleView v = b.view();
    CHECK(b.press(Action::Both, v.targetMs));
    CHECK(!b.view().special && b.view().energy == 0 && b.view().playerHp == v.playerHp - 1 &&
          b.view().enemyHp == v.enemyHp && b.view().lastTiming == Timing::Miss);
    std::puts("PASS special on a dodge cue misfires: energy cleared and 1 HP lost");
    return;
  }
  CHECK(false);
}

void staggeredChord() {
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle b; b.begin(0, seed);
    chargeUntil(b, Action::Attack);
    if (!b.view().active || b.view().enemyHp <= kSpecialDamage) continue;
    const BattleView v = b.view();
    // Right key lands first and resolves alone; the left key within the grace window upgrades it.
    CHECK(b.press(Action::Attack, v.targetMs));
    CHECK(b.view().enemyHp == v.enemyHp - 2 && b.view().energy == kSpecialEnergy);
    CHECK(b.press(Action::Dodge, v.targetMs + kChordGraceMs));
    CHECK(b.view().special && b.view().enemyHp == v.enemyHp - kSpecialDamage && !b.view().energy);
    CHECK(b.view().lastTiming == Timing::Perfect);
    CHECK(!b.press(Action::Both, v.targetMs + kChordGraceMs + 1));
    // A late second key cannot rewrite the beat.
    Battle late; late.begin(0, seed);
    chargeUntil(late, Action::Attack);
    const BattleView w = late.view();
    CHECK(late.press(Action::Attack, w.targetMs) && !late.press(Action::Dodge, w.targetMs + kChordGraceMs + 1));
    CHECK(!late.view().special && late.view().energy == kSpecialEnergy);
    CHECK(!late.press(Action::Attack, w.targetMs + 10));
    std::puts("PASS keys within 80ms upgrade to a special; later or repeated keys are ignored");
    return;
  }
  CHECK(false);
}

void timeoutsAndBounds() {
  Battle b;
  const auto cues = play(b, 3, idle);
  CHECK(b.view().result == Result::Lost && !b.view().playerHp);
  unsigned dodges = 0;
  for (const Cue& c : cues) dodges += c.expected == Action::Dodge;
  CHECK(dodges == kPlayerHp);
  // Perfect play survives every seed within 16 cues; dodging-only never wins.
  for (uint32_t seed = 0; seed < 500; ++seed) {
    Battle p; const auto all = play(p, seed, perfect);
    CHECK(p.view().result == Result::Won && all.size() <= kMaxCues);
    Battle d; play(d, seed, [](Battle& x, const BattleView& v) { if (v.expected == Action::Dodge) x.press(Action::Dodge, v.targetMs); });
    CHECK(d.view().result == Result::Lost && d.view().cueIndex == kMaxCues && d.view().playerHp == kPlayerHp);
  }
  std::puts("PASS idle loses on dodges, perfect play wins every seed, dodge-only times out at 16 cues");
}

void wraparoundAndPause() {
  Battle b; b.begin(UINT32_MAX - 1000, 0);
  CHECK(b.view().targetMs == 499 && !b.view().feint);
  const BattleView first = b.view();
  CHECK(b.press(first.expected, 499)); CHECK(b.view().settledMs == 499);
  CHECK(!b.press(first.expected, 500));
  CHECK(b.requestPause()); b.tick(680); CHECK(b.view().paused && b.view().cueIndex == 1);
  CHECK(b.resume(10000)); CHECK(b.view().targetMs == 11500 && 10000u - b.view().settledMs >= 450);
  b.begin(0); CHECK(!b.press(Action::Dodge, 1700)); CHECK(b.view().cueIndex == 1 && !b.view().consumed);
  CHECK(b.view().settledMs == 1700);
  std::puts("PASS millis wraparound, duplicate press, pause at cue boundary and resume");
}

// Rough difficulty model: a skilled player hits most beats, reads most feints, sometimes slips.
void skilledWinRate() {
  unsigned wins = 0; const unsigned games = 2000;
  for (uint32_t game = 0; game < games; ++game) {
    Battle b; uint32_t rng = game * 2654435761u + 1;
    b.begin(0, mixRandom(game));
    while (b.view().active) {
      const BattleView v = b.view();
      rng = mixRandom(rng); const unsigned r = rng % 100;
      const bool readFeint = !v.feint || (rng >> 8) % 100 < 75;
      const Action act = readFeint ? v.expected : (v.expected == Action::Attack ? Action::Dodge : Action::Attack);
      const uint32_t offset = r < 35 ? 30 : r < 80 ? (v.frenzy ? 100 : 130) : 220;
      if (act == Action::Attack && v.energy >= kSpecialEnergy && readFeint) b.press(Action::Both, v.targetMs + offset);
      else b.press(act, v.targetMs + offset);
      b.tick(v.targetMs + kNormalWindowMs + 1);
    }
    wins += b.view().result == Result::Won;
  }
  // Tuning aid only: device play decides the constants, so no band is asserted.
  std::printf("INFO modelled skilled win rate %u%%\n", wins * 100 / games);
}
}

int main() {
  sameSeedSameCues();
  cuesAreNotStrictlyAlternating();
  feintFlipsLate();
  frenzyAfterHalfHp();
  specialMove();
  specialMisfire();
  staggeredChord();
  timeoutsAndBounds();
  wraparoundAndPause();
  skilledWinRate();
  std::printf("battle: %u checks passed\n", checks);
}
