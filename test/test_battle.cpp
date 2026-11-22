#include "battle.hpp"
#include <cstdio>
#include <cstdlib>
using namespace pokedex::adventure;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace {
void perfect(Battle& b, const BattleView& v) { b.press(v.expected, v.targetMs); }
void charge(Battle& b, const BattleView& v) { b.press(v.expected, v.targetMs + (v.expected == Action::Attack ? 100 : 0)); }
void chargeUntil(Battle& b, Action next) {
  while (b.view().active && (b.view().energy < kSpecialEnergy || b.view().expected != next)) {
    charge(b, b.view()); b.tick(b.view().targetMs + 181);
  }
}

void feintFlipsLate() {
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle b; b.begin(1000, seed);
    while (b.view().active && !b.view().feint) { perfect(b, b.view()); b.tick(b.view().targetMs + 181); }
    if (!b.view().active) continue;
    const BattleView f = b.view();
    CHECK(b.press(shownAction(f, f.targetMs - kCueLeadMs), f.targetMs));
    CHECK(b.view().lastTiming == Timing::Miss && b.view().missReason == MissReason::WrongKey);
    CHECK(f.expected == Action::Attack || b.view().playerHp == f.playerHp - 1);
    return;
  }
  CHECK(false);
}

void frenzyAfterHalfHp() {
  CHECK(battleTiming(1180, 1000) == Timing::Normal && battleTiming(1181, 1000) == Timing::Miss);
  CHECK(battleTiming(930, 1000) == Timing::Perfect && battleTiming(929, 1000) == Timing::Normal);
  CHECK(battleTiming(1140, 1000, true) == Timing::Normal && battleTiming(1141, 1000, true) == Timing::Miss);
  CHECK(battleTiming(1055, 1000, true) == Timing::Perfect && battleTiming(1056, 1000, true) == Timing::Normal);
  Battle b; b.begin(0, 7); bool entered = false;
  while (b.view().active) {
    const BattleView v = b.view();
    if (v.frenzy && v.expected == Action::Attack && !entered) {
      entered = true;
      CHECK(!b.press(Action::Attack, v.targetMs + kFrenzyNormalWindowMs + 1));
      CHECK(b.view().lastTiming == Timing::Miss && b.view().missReason == MissReason::Timeout &&
            b.view().enemyHp == v.enemyHp);
      continue;
    }
    perfect(b, v); b.tick(v.targetMs + kNormalWindowMs + 1);
  }
  CHECK(entered && b.view().result == Result::Won);
}

void specialMisfire() {
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle b; b.begin(0, seed);
    const BattleView first = b.view();
    CHECK(b.press(Action::Both, first.targetMs) && b.view().lastTiming == Timing::Miss &&
          b.view().missReason == MissReason::WrongKey && !b.view().energy);
    b.tick(first.targetMs + 181);
    chargeUntil(b, Action::Dodge);
    if (!b.view().active || b.view().energy < kSpecialEnergy || b.view().playerHp < 2) continue;
    const BattleView v = b.view();
    CHECK(b.press(Action::Both, v.targetMs));
    CHECK(!b.view().special && b.view().energy == 0 && b.view().playerHp == v.playerHp - 1 &&
          b.view().enemyHp == v.enemyHp && b.view().lastTiming == Timing::Miss &&
          b.view().missReason == MissReason::WrongKey);
    return;
  }
  CHECK(false);
}

void staggeredChord() {
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle b; b.begin(0, seed); chargeUntil(b, Action::Attack);
    if (!b.view().active || b.view().enemyHp <= kSpecialDamage) continue;
    const BattleView v = b.view();
    CHECK(b.press(Action::Attack, v.targetMs));
    CHECK(b.press(Action::Dodge, v.targetMs + kChordGraceMs));
    CHECK(b.view().special && b.view().missReason == MissReason::None &&
          b.view().enemyHp == v.enemyHp - kSpecialDamage && !b.view().energy);
    CHECK(!b.press(Action::Both, v.targetMs + kChordGraceMs + 1));
    Battle late; late.begin(0, seed); chargeUntil(late, Action::Attack);
    const BattleView w = late.view();
    CHECK(late.press(Action::Attack, w.targetMs) && !late.press(Action::Dodge, w.targetMs + kChordGraceMs + 1));
    CHECK(!late.view().special && late.view().energy == kSpecialEnergy && late.view().enemyHp == w.enemyHp - 2);
    return;
  }
  CHECK(false);
}

void timeoutsAndBounds() {
  for (uint32_t seed = 0; seed < 20; ++seed) {
    Battle idle; idle.begin(1000, seed);
    while (idle.view().active) idle.tick(idle.view().targetMs + kNormalWindowMs + 1);
    CHECK(idle.view().result == Result::Lost && !idle.view().playerHp);
    Battle dodge; dodge.begin(1000, seed);
    while (dodge.view().active) {
      const auto v = dodge.view();
      if (v.expected == Action::Dodge) dodge.press(Action::Dodge, v.targetMs);
      dodge.tick(v.targetMs + kNormalWindowMs + 1);
    }
    CHECK(dodge.view().result == Result::Lost && dodge.view().playerHp == kPlayerHp);
  }
}

void wraparoundDuplicatePress() {
  Battle b; b.begin(UINT32_MAX - 1000, 0);
  const uint32_t target = kOpeningMs - 1001;
  CHECK(b.press(b.view().expected, target));
  const uint8_t player = b.view().playerHp, enemy = b.view().enemyHp;
  CHECK(!b.press(b.view().expected, target + 1));
  CHECK(b.view().playerHp == player && b.view().enemyHp == enemy);
}

void missReasonClassification() {
  static_assert(kNormalWindowMs == 180 && kPerfectWindowMs == 70, "normal battle windows unchanged");
  static_assert(kFrenzyNormalWindowMs == 140 && kFrenzyPerfectWindowMs == 55, "frenzy battle windows unchanged");
  static_assert(kThrowChargeMs == 1500 && kThrowMinHoldMs == 250, "throw timing constants unchanged");

  Battle b; b.begin(1000, 7);
  CHECK(b.view().missReason == MissReason::None);
  const BattleView c0 = b.view();
  CHECK(b.press(c0.expected, c0.targetMs - kNormalWindowMs - 1));
  CHECK(b.view().lastTiming == Timing::Miss && b.view().missReason == MissReason::Early);

  b.tick(c0.targetMs + kNormalWindowMs + 1);
  const BattleView c1 = b.view();
  const Action wrong = c1.expected == Action::Attack ? Action::Dodge : Action::Attack;
  CHECK(b.press(wrong, c1.targetMs));
  CHECK(b.view().lastTiming == Timing::Miss && b.view().missReason == MissReason::WrongKey);

  b.tick(c1.targetMs + kNormalWindowMs + 1);
  const BattleView c2 = b.view();
  CHECK(b.tick(c2.targetMs + kNormalWindowMs + 1));
  CHECK(b.view().lastTiming == Timing::Miss && b.view().missReason == MissReason::Timeout);

  const BattleView c3 = b.view();
  CHECK(b.press(c3.expected, c3.targetMs));
  CHECK(b.view().lastTiming == Timing::Perfect && b.view().missReason == MissReason::None);

  // Early special misfire on Attack cue and frenzy Early boundary check.
  for (uint32_t seed = 0; seed < 50; ++seed) {
    Battle s; s.begin(0, seed);
    chargeUntil(s, Action::Attack);
    if (!s.view().active || s.view().energy < kSpecialEnergy) continue;
    const BattleView v = s.view();
    const uint32_t win = v.frenzy ? kFrenzyNormalWindowMs : kNormalWindowMs;
    CHECK(s.press(Action::Both, v.targetMs - win - 1));
    CHECK(!s.view().special && !s.view().energy && s.view().lastTiming == Timing::Miss &&
          s.view().missReason == MissReason::Early);
    break;
  }

  Battle f; f.begin(0, 7);
  while (f.view().active && !f.view().frenzy) {
    perfect(f, f.view());
    f.tick(f.view().targetMs + kNormalWindowMs + 1);
  }
  CHECK(f.view().active && f.view().frenzy);
  const BattleView fv = f.view();
  CHECK(f.press(fv.expected, fv.targetMs - kFrenzyNormalWindowMs - 1));
  CHECK(f.view().lastTiming == Timing::Miss && f.view().missReason == MissReason::Early);
}
}

int main() {
  feintFlipsLate(); frenzyAfterHalfHp(); specialMisfire(); staggeredChord();
  timeoutsAndBounds(); wraparoundDuplicatePress(); missReasonClassification();
  std::printf("battle: %u checks passed\n", checks);
}
