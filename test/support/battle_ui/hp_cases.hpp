#pragma once
#include "harness.hpp"

// Custom maximum HP bars and the cross-cue effect frame.
namespace {
void smallMaxHpBars() {
  MemoryStore store; fixture(store); Engine engine(store);
  check(engine.begin() == CommandResult::Applied, "battle fixture begins for small maxima");
  Screens screens; check(screens.begin(), "screens allocate the sprite buffer");
  // Maximum 3 keeps the original three slots: full, damaged and empty.
  BattleSpec three; three.telegraph = TelegraphPattern::DodgeOnly;
  Battle t; t.begin(0, 0, three);
  drawPractice(screens, engine, t.view(), 0);
  hpSegments(91, theme::kMint, 3, 3, "default three-HP bar fills three slots");
  dumpHpRow("hp-max3-full");
  snapshot("battle-hp-max3-full");
  for (unsigned hit = 0; hit < 3; ++hit) {
    const uint32_t target = t.view().targetMs;
    check(t.press(Action::Attack, target), "wrong action misses a three-HP dodge");
    t.tick(target + 181);
    const unsigned remaining = 3 - hit - 1;
    drawPractice(screens, engine, t.view(), t.view().settledMs + kBattleEffectMs);
    hpSegments(91, theme::kMint, remaining, 3, "three-HP bar loses one slot per miss");
    if (remaining == 2) { dumpHpRow("hp-max3-damaged"); snapshot("battle-hp-max3-damaged"); }
  }
  check(t.view().playerHp == 0 && t.view().result == Result::Lost, "three misses empty a three-HP bar");
  drawPractice(screens, engine, t.view(), t.view().settledMs + kBattleEffectMs);
  hpSegments(91, theme::kMint, 0, 3, "empty three-HP bar shows three grey slots");
  dumpHpRow("hp-max3-zero"); snapshot("battle-hp-max3-zero");
  // Maximum 2 must be exactly two slots, never a partially lit three-slot bar.
  BattleSpec two; two.playerHp = two.playerStartHp = 2;
  two.enemyHp = two.enemyStartHp = 2; two.telegraph = TelegraphPattern::DodgeOnly;
  Battle d; d.begin(0, 0, two);
  drawPractice(screens, engine, d.view(), 0);
  hpSegments(91, theme::kMint, 2, 2, "full two-HP bar lights both slots");
  dumpHpRow("hp-max2-full"); snapshot("battle-hp-max2-full");
  { const uint32_t target = d.view().targetMs; d.press(Action::Attack, target); d.tick(target + 181); }
  check(d.view().playerHp == 1, "one miss leaves one of two HP");
  drawPractice(screens, engine, d.view(), d.view().settledMs + kBattleEffectMs);
  hpSegments(91, theme::kMint, 1, 2, "damaged two-HP bar lights one slot");
  dumpHpRow("hp-max2-damaged"); snapshot("battle-hp-max2-damaged");
  { const uint32_t target = d.view().targetMs; d.press(Action::Attack, target); d.tick(target + 181); }
  check(d.view().playerHp == 0 && d.view().result == Result::Lost, "two misses empty a two-HP bar");
  drawPractice(screens, engine, d.view(), d.view().settledMs + kBattleEffectMs);
  hpSegments(91, theme::kMint, 0, 2, "empty two-HP bar shows two grey slots");
  dumpHpRow("hp-max2-zero"); snapshot("battle-hp-max2-zero");
  // A maximum of one has a single slot for both full and empty.
  BattleSpec one; one.playerHp = one.playerStartHp = 1;
  one.enemyHp = one.enemyStartHp = 1; one.telegraph = TelegraphPattern::AttackOnly;
  Battle o; o.begin(0, 0, one);
  drawPractice(screens, engine, o.view(), 0);
  hpSegments(91, theme::kMint, 1, 1, "full one-HP bar is one mint slot");
  hpSegments(282, theme::kGold, 1, 1, "full one-HP enemy bar is one gold slot");
  dumpHpRow("hp-max1-full"); snapshot("battle-hp-max1-full");
  { const uint32_t target = o.view().targetMs; o.press(Action::Attack, target + 100); o.tick(target + 181); }
  check(o.view().enemyHp == 0 && o.view().result == Result::Won, "one normal hit empties a one-HP enemy");
  drawPractice(screens, engine, o.view(), o.view().settledMs + kBattleEffectMs);
  hpSegments(282, theme::kGold, 0, 1, "empty one-HP enemy bar is one grey slot");
  dumpHpRow("hp-max1-zero"); snapshot("battle-hp-max1-zero");
  BattleSpec single; single.playerHp = single.playerStartHp = 1;
  single.enemyHp = single.enemyStartHp = 1; single.telegraph = TelegraphPattern::DodgeOnly;
  Battle s; s.begin(0, 0, single);
  { const uint32_t target = s.view().targetMs; s.press(Action::Attack, target); s.tick(target + 181); }
  check(s.view().playerHp == 0 && s.view().result == Result::Lost, "one miss empties a one-HP player");
  drawPractice(screens, engine, s.view(), s.view().settledMs + kBattleEffectMs);
  hpSegments(91, theme::kMint, 0, 1, "empty one-HP player bar is one grey slot");
  dumpHpRow("hp-max1-player-zero"); snapshot("battle-hp-max1-player-zero");
}
void customHpBars() {
  MemoryStore store; fixture(store); Engine engine(store);
  check(engine.begin() == CommandResult::Applied, "battle fixture begins for direct rendering");
  Screens screens; check(screens.begin(), "screens allocate the sprite buffer");
  // Default 3/3 keeps the original three segments.
  Battle plain; plain.begin(0);
  drawPractice(screens, engine, plain.view(), 0);
  hp(3, 3);
  // Maximum and start HP differ, so the bar shows the remaining fraction.
  BattleSpec spec; spec.playerHp = 5; spec.enemyHp = 4;
  spec.playerStartHp = 3; spec.enemyStartHp = 4;
  Battle b; b.begin(0, 0, spec);
  drawPractice(screens, engine, b.view(), 0);
  hpBar(91, theme::kMint, 3, 5, "player bar shows three of five maximum HP");
  hpBar(282, theme::kGold, 4, 4, "enemy bar shows a full four-HP track");
  snapshot("battle-hp-custom-start");
  // Five -> four -> three -> zero remaining HP must each render differently.
  BattleSpec drain; drain.playerHp = 5; drain.enemyHp = 4;
  drain.playerStartHp = 5; drain.enemyStartHp = 4;
  drain.telegraph = TelegraphPattern::DodgeOnly;
  Battle d; d.begin(0, 0, drain);
  for (unsigned remaining = 5; remaining > 0; --remaining) {
    const uint32_t target = d.view().targetMs;
    check(d.view().playerHp == remaining, "start and settled HP follow the drain order");
    drawPractice(screens, engine, d.view(), d.view().settledMs + kBattleEffectMs);
    hpBar(91, theme::kMint, remaining, 5, "player bar tracks every lost HP");
    if (remaining == 5) snapshot("battle-hp-custom-5");
    if (remaining == 4) snapshot("battle-hp-custom-4");
    if (remaining == 3) snapshot("battle-hp-custom-3");
    check(d.press(Action::Attack, target), "wrong action lands a missed dodge");
    d.tick(target + 181);
  }
  check(d.view().playerHp == 0 && d.view().result == Result::Lost, "five missed dodges empty a five-HP bar");
  drawPractice(screens, engine, d.view(), d.view().settledMs + kBattleEffectMs);
  hpBar(91, theme::kMint, 0, 5, "empty custom bar shows only its track");
  snapshot("battle-hp-custom-0");
  // The enemy bar drains one point per normal hit on a four-HP maximum.
  BattleSpec hit; hit.enemyHp = 4; hit.playerHp = 3;
  hit.enemyStartHp = 4; hit.playerStartHp = 3;
  hit.telegraph = TelegraphPattern::AttackOnly;
  Battle h; h.begin(0, 0, hit);
  for (unsigned remaining = 4; remaining > 0; --remaining) {
    const uint32_t target = h.view().targetMs;
    check(h.view().enemyHp == remaining, "enemy HP follows the hit order");
    drawPractice(screens, engine, h.view(), h.view().settledMs + kBattleEffectMs);
    hpBar(282, theme::kGold, remaining, 4, "enemy bar tracks every lost HP");
    if (remaining == 3) snapshot("battle-hp-enemy-3");
    check(h.press(Action::Attack, target + 100), "normal attack lands inside the window");
    h.tick(target + 181);
  }
  check(h.view().enemyHp == 0 && h.view().result == Result::Won, "four normal hits empty a four-HP bar");
  drawPractice(screens, engine, h.view(), h.view().settledMs + kBattleEffectMs);
  hpBar(282, theme::kGold, 0, 4, "empty enemy bar shows only its track");
}
// A settled action must survive the cue advance: the cross-cue effect frame has
// to match the frame drawn while the same beat was still settled.
void continuousTelegraphRender() {
  MemoryStore store; fixture(store); Engine engine(store);
  check(engine.begin() == CommandResult::Applied, "battle fixture begins for continuous rendering");
  Screens screens; check(screens.begin(), "screens allocate the sprite buffer");
  for (TelegraphPattern pattern : {TelegraphPattern::AttackOnly, TelegraphPattern::DodgeOnly}) {
    BattleSpec spec; spec.telegraph = pattern;
    Battle b; b.begin(0, 0, spec);
    const uint32_t target = b.view().targetMs;
    const Action performed = b.view().expected;
    check(b.press(performed, target) && b.view().lastTiming == Timing::Perfect,
          "continuous pattern accepts its telegraph as perfect");
    const BattleView settled = b.view();
    b.tick(target + 181);
    const BattleView advanced = b.view();
    check(!advanced.consumed && advanced.cueIndex == 1 &&
          battleEffectActive(advanced, target + 181) &&
          battleEffectAction(advanced) == performed,
          "cross-cue effect keeps the settled action");
    drawPractice(screens, engine, settled, target + 181);
    const std::vector<uint16_t> settledFrame = region(88, 342);
    drawPractice(screens, engine, advanced, target + 181);
    check(region(88, 342) == settledFrame, "cross-cue effect draws the settled action pixels");
    snapshot(pattern == TelegraphPattern::AttackOnly ? "battle-attackonly-cross-cue"
                                                     : "battle-dodgeonly-cross-cue");
  }
}
}
