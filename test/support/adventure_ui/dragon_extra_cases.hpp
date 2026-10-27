#pragma once
#include "dragon_cases.hpp"

// Upgrade, defeat, capture-retry, zero-ball and replay Runtime cases. They
// reuse the fixtures, input drivers and pixel oracles from dragon_cases.hpp.
namespace {
void dragonUpgradeDisabled() {
  // The reachable dead-row page needs two stage wins: buy the guard after stage
  // 0, then win stage 1 at full HP. Stage 2's wait keeps only the window row.
  MemoryStore store; dragonStore(store, 6);
  TestPlatform p; Runtime r(store, p); start(r);
  p.seed = 77;
  openDragon(r, p); select(r, p, 1); select(r, p, 0);
  dragonBattleWin(r, p);
  check(r.screen().page == Page::DragonUpgrade, "stage 0 win asks for the first upgrade");
  check(dragonTrailing("已满", 238, theme::kMuted), "heal stays disabled at full HP");
  select(r, p, 2);
  check(r.screen().page == Page::DragonStage && r.engine().view().dragonTrial.guard == 1,
        "guard is taken after stage 0");
  select(r, p, 0);
  dragonBattleWin(r, p);
  check(r.screen().page == Page::DragonUpgrade, "stage 1 win asks for the second upgrade");
  check(r.engine().view().dragonTrial.stageIndex == 2 &&
        r.engine().view().dragonTrial.stageStartHp == 3 &&
        r.engine().view().dragonTrial.guard == 1 &&
        r.engine().view().dragonTrial.normalBonus == 0,
        "the saved wait is the reachable full-HP guard snapshot");
  check(dragonTrailing("已满", 238, theme::kMuted), "heal stays disabled at full HP");
  check(dragonTrailing("已获得", 374, theme::kMuted), "taken guard stays disabled");
  check(dragonTrailing("210/80", 306, theme::kText), "window row states the resulting windows");
  published(r, "dragon-upgrade-disabled");
  select(r, p, 1);
  check(r.screen().page == Page::DragonStage &&
        r.engine().view().dragonTrial.normalBonus == 30 &&
        r.engine().view().dragonTrial.perfectBonus == 10,
        "the remaining window row still commits");
  check(ruinsLine("窗口 210/80ms · 护盾 1", 194, theme::kMuted, false),
        "stage sheet shows the upgraded windows and kept guard");
}

void dragonDefeatAndRetry() {
  MemoryStore store; dragonStore(store, 6);
  TestPlatform p; Runtime r(store, p); start(r);
  openDragon(r, p); select(r, p, 0); select(r, p, 0);
  dragonBattleWin(r, p);
  select(r, p, 1);  // window upgrade
  select(r, p, 0);
  const uint64_t id = r.engine().view().encounter.id;
  const uint32_t seed = r.engine().view().encounter.seed;
  dragonBattleLose(r, p);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::Lost,
        "defeat reaches the result page");
  check(menuCount(r.screen(), r.engine()) == 2, "defeat offers retry and return");
  published(r, "dragon-defeat");
  select(r, p, 0);
  check(r.screen().page == Page::DragonStage, "retry returns to the same stage");
  check(r.engine().view().encounter.id == id && r.engine().view().encounter.seed == seed,
        "battle retry reuses the activity ID and seed");
  check(r.engine().view().dragonTrial.normalBonus == 30, "retry keeps the chosen upgrade");
}

void dragonThrowRetry() {
  MemoryStore store; dragonStore(store, 6);
  TestPlatform p; Runtime r(store, p); start(r);
  p.seed = dragonUiSeed(7, 147, false);
  openDragon(r, p); select(r, p, 0); select(r, p, 0);
  dragonBattleWin(r, p);
  select(r, p, 2);  // guard
  select(r, p, 0);
  dragonBattleWin(r, p);
  const uint64_t id = r.engine().view().encounter.id;
  check(r.screen().page == Page::Throw, "final win opens the throw page");
  dragonThrow(r, p);
  check(r.engine().view().phase == Phase::ThrowReady, "first miss keeps the encounter");
  edge(r, p, false, true);
  throwAt(r, p, r.engine().throwView().targetMs);
  idle(r, 5000);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::Escaped,
        "two misses close the capture");
  published(r, "dragon-escaped");
  p.seed = 4321;
  select(r, p, 0);
  check(r.screen().page == Page::DragonStage, "capture retry returns to the final stage");
  check(r.engine().view().encounter.id == id + 1 &&
        r.engine().view().encounter.seed == 4321, "capture retry allocates a new ID and seed");
  check(r.engine().view().dragonTrial.stageIndex == 1 &&
        r.engine().view().dragonTrial.guard == 1, "capture retry keeps the final snapshot");
  check(r.engine().view().ballsSpent == 2, "capture retry never refunds spent balls");
}

void dragonZeroBalls() {
  MemoryStore store; dragonStore(store, 6, 0, true);
  TestPlatform p; Runtime r(store, p); start(r);
  p.seed = dragonUiSeed(7, 147, true);
  openDragon(r, p); select(r, p, 0); select(r, p, 0);
  dragonBattleWin(r, p);
  select(r, p, 2);
  select(r, p, 0);
  dragonBattleWin(r, p);
  check(r.screen().page == Page::Throw && availableBalls(r.engine().view()) == 0,
        "zero balls keeps the won final stage");
  check(ruinsLine("暂无精灵球", 238, theme::kGold, true), "throw page states there is no ball");
  published(r, "dragon-zero-balls");
  const uint64_t id = r.engine().view().encounter.id;
  for (unsigned guard = 0; r.screen().selected != 1 && guard < 4; ++guard) edge(r, p, true, false);
  check(r.screen().page == Page::Throw && r.screen().selected == 1,
        "throw page exposes keep-and-walk");
  edge(r, p, true, true);
  check(r.screen().page == Page::Home, "leaving the throw keeps the encounter");
  // Walk the same Runtime until the next ball is earned; the saved throw must
  // stay pending instead of being replaced by a new ordinary encounter.
  const uint64_t before = r.engine().view().totalSteps;
  walk(r, p, 1008);
  check(r.engine().view().encounter.id == id && r.engine().view().phase == Phase::ThrowReady,
        "walking keeps the same won final stage");
  check(r.engine().view().totalSteps >= before + 1000 && availableBalls(r.engine().view()) >= 1,
        "one more ball is earned after 1000 steps");
  if (r.dimmed()) edge(r, p, false, true);
  check(!r.dimmed(), "the walk wakes before reopening the hub");
  openMenu(r, p); select(r, p, 8);
  select(r, p, 0);
  check(r.screen().page == Page::Throw && r.engine().view().encounter.id == id,
        "hub reopens the same throw with the earned ball");
  published(r, "dragon-zero-balls-resume");
  dragonThrow(r, p);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::Captured,
        "the banked ball captures the waiting dragon");
  check(r.engine().view().captureCounts[146] == 1 &&
        r.engine().view().encounter.usedBalls == 1,
        "the resumed throw joins the unified account");
  published(r, "dragon-zero-balls-captured");
  captureCardBadges(r, 8);
}

void dragonReplay() {
  MemoryStore store; dragonStore(store, 6, 147);
  TestPlatform p; Runtime r(store, p); start(r);
  openDragon(r, p); select(r, p, 0);
  check(r.screen().page == Page::DragonStage, "captured target can be replayed in an open window");
  select(r, p, 0);
  dragonBattleWin(r, p);
  select(r, p, 1);
  select(r, p, 0);
  const uint32_t counts = r.engine().view().captureCounts[146];
  dragonBattleWin(r, p);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::DragonCleared,
        "replay win skips the throw");
  check(menuCount(r.screen(), r.engine()) == 1, "replay result offers a single confirm");
  check(ruinsLine("不改变图鉴与捕获次数", 226, theme::kText, false),
        "replay result states there is no reward");
  published(r, "dragon-replay-cleared");
  select(r, p, 0);
  check(r.screen().page == Page::Dragon && r.engine().view().captureCounts[146] == counts,
        "replay leaves the capture account untouched");
}

void dragonExtraCases() {
  dragonUpgradeDisabled();
  dragonDefeatAndRetry();
  dragonThrowRetry();
  dragonZeroBalls();
  dragonReplay();
}
}
