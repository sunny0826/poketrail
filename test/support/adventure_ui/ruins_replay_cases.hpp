#pragma once
#include "ruins_harness.hpp"

// Retreat retry, captured replay, zero-ball wait and mixed target states.
namespace {
void ruinsBossRetreatRetry() {
  MemoryStore store; ruinsStore(store, true, 150);
  TestPlatform p; Runtime r(store, p); start(r);
  openRuins(r, p);
  select(r, p, 1);
  published(r, "ruins-boss-150");
  check(ruinsHeading(speciesName(150)), "five-HP boss detail renders its heading");
  ruinsBossSpec("生命 我方3 / 首领5", "节拍 1.3 秒 · 12 拍", "节奏 连续攻击 · 判定 180/70ms");
  select(r, p, 0);
  select(r, p, 0);
  check(r.screen().page == Page::Battle && r.engine().battleView().enemyMaxHp == 5 &&
        r.engine().battleView().beatMs == 1300, "five-HP boss renders its real maximum and beat");
  published(r, "ruins-boss-150-battle");
  idle(r, 180); tap(r, p, 233, 30);
  for (unsigned i = 0; r.screen().page == Page::Battle && i < 150; ++i) frame(r);
  check(r.screen().page == Page::Pause, "boss battle pauses after the current cue");
  published(r, "ruins-boss-pause");
  select(r, p, 1);
  idle(r, 100);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::Retreated,
        "retreating the boss lands on the retryable result page");
  published(r, "ruins-boss-retreated");
  check(ruinsHeading("已经撤退") && ruinsLine("线索与解锁保留", 226, theme::kText),
        "retreat result explains that progress is kept");
  const uint64_t id = r.engine().view().encounter.id;
  select(r, p, 0);
  check(r.screen().page == Page::Encounter && r.engine().view().encounter.id == id + 1,
        "retry creates a new encounter instead of overwriting the finished one");
  published(r, "ruins-retry");
  select(r, p, 2);
  check(r.screen().page == Page::Ruins && r.engine().view().ruins.unlocked &&
        r.engine().view().ruins.lastTarget == 150, "leaving the retry keeps unlock and target");
}

void ruinsCapturedReplayAndZeroBalls() {
  {
    MemoryStore store; ruinsStore(store, true, 150, 150);
    TestPlatform p; Runtime r(store, p); start(r);
    openRuins(r, p); select(r, p, 1);
    published(r, "ruins-captured-detail");
    select(r, p, 0); select(r, p, 0);
    ruinsWin(r, p);
    check(r.screen().page == Page::Result && r.engine().view().result == Result::RuinsCleared,
          "replaying a captured boss skips the throw stage");
    check(r.engine().view().captureCounts[149] == 1 && r.engine().view().ballsSpent == 0,
          "replay win changes no capture count and spends no ball");
    published(r, "ruins-cleared");
    check(ruinsHeading("已完成 · 无奖励") && ruinsLine("不改变图鉴与捕获次数", 258, theme::kText),
          "captured replay result explains that nothing is awarded");
    select(r, p, 0);
    check(r.screen().page == Page::Ruins && r.engine().view().phase == Phase::Walking,
          "capture confirmation returns to the walking hub");
  }
  {
    MemoryStore store; ruinsStore(store, true, 249, 0, true);
    TestPlatform p; Runtime r(store, p); start(r);
    openRuins(r, p); select(r, p, 1); select(r, p, 0); select(r, p, 0);
    ruinsWin(r, p);
    check(r.screen().page == Page::Throw && availableBalls(r.engine().view()) == 0,
          "zero-ball boss win still opens the throw page with its victory progress");
    published(r, "ruins-zero-balls");
    check(ruinsHeading(speciesName(249)) && ruinsText("每 1000 步获得一球", 80, theme::kGold, false),
          "zero-ball throw page states the ball rule");
    select(r, p, 0);
    check(r.screen().page == Page::Throw && r.engine().view().phase == Phase::ThrowReady,
          "no-ball prepare is refused without losing the win");
    Runtime restart(store, p); start(restart);
    check(restart.screen().page == Page::Throw && restart.engine().view().phase == Phase::ThrowReady,
          "restart keeps the won boss waiting for a ball");
    published(restart, "ruins-zero-balls-restart");
  }
}

void ruinsTargetStates() {
  MemoryStore store; ruinsMixedTargetsStore(store);
  TestPlatform p; Runtime r(store, p); start(r);
  openRuins(r, p); select(r, p, 0);
  check(r.screen().page == Page::RuinsTargets, "target list opens while a boss encounter waits");
  published(r, "ruins-targets-mixed");
  const auto& save = r.engine().view();
  check(save.captureCounts[149] == 1 && save.encounter.species == 251,
        "mixed list fixture has one completed and one current target");
  select(r, p, 1);
  check(r.engine().view().ruins.lastTarget == 251 && r.engine().view().encounter.species == 251,
        "selecting another target while a boss waits neither switches nor starts a fight");
  published(r, "ruins-targets-blocked");
  select(r, p, 11); select(r, p, 1);
  check(r.screen().page == Page::Encounter, "hub continue entry returns to the waiting encounter");
}
}
