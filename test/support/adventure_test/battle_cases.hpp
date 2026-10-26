#pragma once
#include "harness.hpp"
#include "battle_presentation.hpp"

// Battle settlement, pause and configurable spec equivalence.
static bool sameView(const BattleView& x, const BattleView& y) {
  return x.active == y.active && x.paused == y.paused &&
         x.pauseRequested == y.pauseRequested && x.consumed == y.consumed &&
         x.playerHp == y.playerHp && x.enemyHp == y.enemyHp &&
         x.playerMaxHp == y.playerMaxHp && x.enemyMaxHp == y.enemyMaxHp &&
         x.cueIndex == y.cueIndex && x.targetMs == y.targetMs &&
         x.settledMs == y.settledMs && x.expected == y.expected &&
         x.lastTiming == y.lastTiming && x.lastAction == y.lastAction &&
         x.lastActionValid == y.lastActionValid && x.result == y.result &&
         x.beatMs == y.beatMs && x.normalWindowMs == y.normalWindowMs &&
         x.perfectWindowMs == y.perfectWindowMs && x.guard == y.guard &&
         x.perfectCount == y.perfectCount;
}
static void battles() {
  Battle b; b.begin(UINT32_MAX - 1000, 0);
  CHECK(b.view().targetMs == 499);
  CHECK(b.press(Action::Both, 499)); CHECK(b.view().enemyHp == 3);
  CHECK(b.view().settledMs == 499);
  CHECK(!b.press(Action::Attack, 500));
  CHECK(b.requestPause()); b.tick(680); CHECK(b.view().paused && b.view().cueIndex == 1);
  CHECK(b.resume(10000)); CHECK(b.view().targetMs == 11500 && 10000u - b.view().settledMs >= 450);
  CHECK(b.requestPause()); b.tick(11681); CHECK(b.view().playerHp == 2 && b.view().paused);
  CHECK(b.view().settledMs == 11681);
  CHECK(b.resume(20000)); b.tick(100000); CHECK(b.view().result == Result::Lost && b.view().settledMs == 100000);
  b.begin(UINT32_MAX - 1499); CHECK(b.press(Action::Attack, 0)); CHECK(b.view().settledMs == 0);
  CHECK(b.view().enemyHp == 1); b.tick(181); CHECK(b.view().settledMs == 0 && b.view().cueIndex == 1);
  b.begin(0); CHECK(!b.press(Action::Dodge, 1700)); CHECK(b.view().cueIndex == 1 && !b.view().consumed);
  CHECK(b.view().settledMs == 1700);
  b.begin(0);
  for (unsigned i = 0; i < 12; ++i) {
    const uint32_t target = b.view().targetMs;
    if (i % 2) CHECK(b.press(Action::Dodge, target));
    b.tick(target + 181);
  }
  CHECK(b.view().result == Result::Lost && b.view().playerHp == 3);
  CHECK(battleTiming(1180, 1000) == Timing::Normal && battleTiming(1181, 1000) == Timing::Miss);
  CHECK(battleTiming(930, 1000) == Timing::Perfect && battleTiming(929, 1000) == Timing::Normal);
  World w; w.steps(5000); CHECK(w.e.startBattle(w.now) == CommandResult::Applied);
  w.now += 30000; CHECK(w.e.tick(w.now, w.clock()) == CommandResult::Applied);
  CHECK(w.e.view().result == Result::Lost);
  CHECK(w.e.beginThrow(AttemptKey(w.e.view().encounter.id, 1), w.now) == CommandResult::InvalidPhase);
  World rebooted; rebooted.steps(5000); const uint64_t id = rebooted.e.view().encounter.id;
  CHECK(rebooted.e.startBattle(100) == CommandResult::Applied);
  CHECK(rebooted.e.requestPause() == CommandResult::Applied);
  CHECK(rebooted.e.tick(5000, rebooted.clock()) == CommandResult::Applied);
  CHECK(rebooted.e.checkpoint() == CommandResult::Applied);
  Engine restore(rebooted.store); CHECK(restore.begin() == CommandResult::Applied);
  CHECK(restore.view().phase == Phase::EncounterReady && restore.view().encounter.id == id && restore.view().explorationCredit == 0);
  std::puts("PASS battle first input, wrong/both, wraparound, pause, 6 rounds, lose and reboot");
}
static void battleSpec() {
  // The default entry point must stay field-for-field identical to the explicit
  // default spec, including the seed jitter, pause and rollover behaviour.
  Battle legacy, explicitDefault;
  legacy.begin(1000, 7);
  explicitDefault.begin(1000, 7, BattleSpec());
  CHECK(sameView(legacy.view(), explicitDefault.view()));
  for (unsigned cue = 0; cue < 12; ++cue) {
    const uint32_t target = legacy.view().targetMs;
    CHECK(legacy.press(legacy.view().expected, target) ==
          explicitDefault.press(explicitDefault.view().expected, target));
    CHECK(sameView(legacy.view(), explicitDefault.view()));
    legacy.tick(target + 181);
    explicitDefault.tick(target + 181);
    CHECK(sameView(legacy.view(), explicitDefault.view()));
    if (!legacy.view().active) break;
  }
  CHECK(legacy.view().result == Result::Won && explicitDefault.view().result == Result::Won);
  Battle paused1, paused2;
  paused1.begin(0); paused2.begin(0, 0, BattleSpec());
  CHECK(paused1.requestPause() && paused2.requestPause());
  paused1.tick(1681); paused2.tick(1681);
  CHECK(sameView(paused1.view(), paused2.view()));
  CHECK(paused1.resume(10000) && paused2.resume(10000));
  CHECK(sameView(paused1.view(), paused2.view()));

  // Custom max/start HP, cue limit, beat, windows, telegraph and one guard.
  BattleSpec spec;
  spec.playerHp = 5; spec.enemyHp = 4;
  spec.playerStartHp = 3; spec.enemyStartHp = 3;
  spec.cueLimit = 8; spec.beatMs = 1000;
  spec.normalWindowMs = 100; spec.perfectWindowMs = 30;
  spec.telegraph = TelegraphPattern::AlternateDodgeFirst;
  spec.missedDodgeGuard = 1;
  Battle b; b.begin(0, 0, spec);
  CHECK(b.view().playerHp == 3 && b.view().enemyHp == 3);
  BattleSpec clamped = spec; clamped.playerStartHp = 9; clamped.enemyStartHp = 9;
  Battle overfull; overfull.begin(0, 0, clamped);
  CHECK(overfull.view().playerHp == 5 && overfull.view().enemyHp == 4);
  CHECK(b.view().beatMs == 1000 && b.view().normalWindowMs == 100 &&
        b.view().perfectWindowMs == 30 && b.view().guard == 1);
  CHECK(b.view().expected == Action::Dodge && b.view().targetMs == 1000);
  CHECK(b.press(Action::Dodge, 1000) && b.view().lastTiming == Timing::Perfect);
  CHECK(b.view().perfectCount == 1 && b.view().playerHp == 3 && b.view().guard == 1);
  b.tick(1101);
  CHECK(b.view().cueIndex == 1 && b.view().playerHp == 3 && b.view().guard == 1);
  b.tick(2101);
  CHECK(b.view().cueIndex == 2 && b.view().playerHp == 3 && b.view().guard == 1);
  CHECK(b.view().expected == Action::Dodge);
  b.tick(3101);
  CHECK(b.view().cueIndex == 3 && b.view().playerHp == 3 && b.view().guard == 0 &&
        b.view().expected == Action::Attack);
  CHECK(b.press(Action::Attack, 4000) && b.view().lastTiming == Timing::Perfect);
  CHECK(b.view().perfectCount == 2 && b.view().enemyHp == 1 && b.view().active);
  b.tick(4101);
  CHECK(b.view().cueIndex == 4 && b.view().expected == Action::Dodge);
  b.tick(5101);
  CHECK(b.view().cueIndex == 5 && b.view().playerHp == 2 && b.view().guard == 0);
  CHECK(b.press(Action::Attack, 6000) && b.view().lastTiming == Timing::Perfect);
  CHECK(b.view().perfectCount == 3 && b.view().enemyHp == 0 && b.view().result == Result::Won);

  // Custom windows decide the timing tier, not the default 180/70.
  Battle c; c.begin(0, 0, spec);
  CHECK(c.press(Action::Dodge, 1030) && c.view().lastTiming == Timing::Perfect);
  Battle normal; normal.begin(0, 0, spec);
  CHECK(normal.press(Action::Dodge, 1060) && normal.view().lastTiming == Timing::Normal);
  CHECK(normal.view().perfectCount == 0);
  Battle wrong; wrong.begin(0, 0, spec);
  CHECK(wrong.press(Action::Attack, 1000) && wrong.view().lastTiming == Timing::Miss);
  CHECK(wrong.view().perfectCount == 0 && wrong.view().enemyHp == 3);
  CHECK(battleTiming(1030, 1000, 30, 100) == Timing::Perfect);
  CHECK(battleTiming(1031, 1000, 30, 100) == Timing::Normal);
  CHECK(battleTiming(1101, 1000, 30, 100) == Timing::Miss);
  CHECK(battleTiming(930, 1000) == Timing::Perfect && battleTiming(929, 1000) == Timing::Normal);

  // A settled action must survive the cue advance for every pattern: the next
  // cue never identifies what just happened in AttackOnly/DodgeOnly.
  for (TelegraphPattern pattern : {TelegraphPattern::AttackOnly, TelegraphPattern::DodgeOnly,
                                   TelegraphPattern::Alternate, TelegraphPattern::AlternateDodgeFirst}) {
    BattleSpec continuous;
    continuous.telegraph = pattern;
    Battle settled;
    settled.begin(0, 0, continuous);
    const uint32_t settledTarget = settled.view().targetMs;
    CHECK(settled.press(settled.view().expected, settledTarget) && settled.view().consumed);
    const Action performed = settled.view().expected;
    CHECK(pokedex::ui::battleEffectActive(settled.view(), settledTarget));
    CHECK(pokedex::ui::battleEffectAction(settled.view()) == performed);
    settled.tick(settledTarget + 181);
    CHECK(!settled.view().consumed && settled.view().cueIndex == 1);
    CHECK(pokedex::ui::battleEffectActive(settled.view(), settledTarget + 181));
    CHECK(pokedex::ui::battleEffectAction(settled.view()) == performed);
  }

  // The cue limit ends the battle after exactly that many beats.
  BattleSpec shortSpec = spec;
  shortSpec.cueLimit = 4;
  Battle limited; limited.begin(0, 0, shortSpec);
  limited.tick(4101);
  CHECK(limited.view().result == Result::Lost && limited.view().cueIndex == 4);
  CHECK(limited.view().playerHp == 2 && limited.view().guard == 0);
  std::puts("PASS default Battle equivalence and configurable spec, guard, perfect count and limit");
}
