#pragma once
#include "ruins_harness.hpp"

// R1 review regressions: retryable result menu and deferred throw resume.
namespace {
void ruinsCapturedFailureMenu() {
  // A captured target that retreats keeps both actions; retry must start a new
  // attempt, never acknowledge the failure as if it were a finished capture.
  {
    MemoryStore store; ruinsStore(store, true, 150, 150);
    TestPlatform p; Runtime r(store, p); start(r);
    openRuins(r, p); select(r, p, 1); select(r, p, 0); select(r, p, 0);
    idle(r, 180); tap(r, p, 233, 30);
    for (unsigned i = 0; r.screen().page == Page::Battle && i < 150; ++i) frame(r);
    check(r.screen().page == Page::Pause, "captured replay pauses after the current cue");
    select(r, p, 1);
    idle(r, 100);
    check(r.screen().page == Page::Result && r.engine().view().result == Result::Retreated,
          "captured replay retreat reaches the result page");
    check(menuCount(r.screen(), r.engine()) == 2, "captured retreat keeps both result actions");
    published(r, "ruins-captured-retreat");
    check(ruinsText("重试本阶段", 306, theme::kGold, true, 10000),
          "captured retreat draws the retry action");
    check(ruinsText("返回遗迹", 374, theme::kText, true, 10000),
          "captured retreat draws the back action");
    check(!ruinsText("确认结果 · 返回遗迹", 306, theme::kGold, true, 10000),
          "captured retreat no longer offers a single confirm row");
    const uint64_t id = r.engine().view().encounter.id;
    select(r, p, 0);
    check(r.screen().page == Page::Encounter && r.engine().view().encounter.id == id + 1,
          "captured retreat retry starts a new attempt instead of acknowledging");
  }
  // A captured target that loses also keeps the two actions, and back really
  // acknowledges the failed run.
  {
    MemoryStore store; ruinsStore(store, true, 150, 150);
    TestPlatform p; Runtime r(store, p); start(r);
    openRuins(r, p); select(r, p, 1); select(r, p, 0); select(r, p, 0);
    for (unsigned i = 0; r.engine().view().phase == Phase::Battle && i < 1600; ++i) frame(r);
    idle(r, 600);
    check(r.screen().page == Page::Result && r.engine().view().result == Result::Lost,
          "captured replay can lose the fight");
    check(menuCount(r.screen(), r.engine()) == 2, "captured loss keeps both result actions");
    published(r, "ruins-captured-lost");
    check(ruinsText("重试本阶段", 306, theme::kGold, true, 10000) &&
          ruinsText("返回遗迹", 374, theme::kText, true, 10000),
          "captured loss draws retry and back");
    select(r, p, 1);
    check(r.screen().page == Page::Ruins && r.engine().view().phase == Phase::Walking &&
          r.engine().view().ruins.unlocked && r.engine().view().captureCounts[149] == 1,
          "captured loss back acknowledges without losing progress");
  }
}
void ruinsDeferredThrowResume() {
  // Zero balls: the deferred throw waits for a ball; the hub entry must resume
  // the throw page instead of offering a battle that the phase cannot start.
  {
    MemoryStore store; ruinsStore(store, true, 249, 0, true);
    TestPlatform p; Runtime r(store, p); start(r);
    openRuins(r, p); select(r, p, 1); select(r, p, 0); select(r, p, 0);
    ruinsWin(r, p);
    check(r.screen().page == Page::Throw && r.engine().view().phase == Phase::ThrowReady,
          "zero-ball win waits on the throw page");
    const uint64_t id = r.engine().view().encounter.id;
    const uint32_t seed = r.engine().view().encounter.seed;
    throwMenuItem(r, p, 1);
    check(r.screen().page == Page::Home && r.engine().view().phase == Phase::ThrowReady,
          "keeping the encounter leaves the win waiting on the home page");
    openRuins(r, p);
    select(r, p, 1);
    check(r.screen().page == Page::Throw, "ruins hub resumes the deferred throw, not the battle page");
    check(r.engine().view().encounter.id == id && r.engine().view().encounter.seed == seed &&
          r.engine().view().encounter.usedBalls == 0 && r.engine().view().phase == Phase::ThrowReady,
          "deferred resume keeps the encounter id, seed and ball account");
    published(r, "ruins-resume-zero-balls");
    throwMenuItem(r, p, 0);
    check(r.screen().page == Page::Throw && r.engine().view().phase == Phase::ThrowReady &&
          r.screen().command == CommandResult::NoBalls,
          "resumed zero-ball throw still refuses to throw without losing the win");
  }
  // One ball already used: the same entry must keep that attempt count.
  {
    MemoryStore store; ruinsStore(store, true, 251);
    TestPlatform p; Runtime r(store, p); start(r);
    openRuins(r, p); select(r, p, 1); select(r, p, 0); select(r, p, 0);
    ruinsWin(r, p);
    check(r.screen().page == Page::Throw && availableBalls(r.engine().view()) > 0,
          "funded boss win offers a real throw");
    throwMenuItem(r, p, 0);
    idle(r, 1800);
    check(r.engine().view().encounter.usedBalls == 1 && r.screen().page == Page::Throw,
          "a timed-out throw spends one ball and keeps the encounter");
    const uint64_t id = r.engine().view().encounter.id;
    const uint32_t seed = r.engine().view().encounter.seed;
    const uint64_t balls = availableBalls(r.engine().view());
    throwMenuItem(r, p, 1);
    check(r.screen().page == Page::Home, "keeping a used-ball encounter returns to walking");
    openRuins(r, p);
    select(r, p, 1);
    check(r.screen().page == Page::Throw, "ruins hub resumes a used-ball throw");
    check(r.engine().view().encounter.id == id && r.engine().view().encounter.seed == seed &&
          r.engine().view().encounter.usedBalls == 1 && availableBalls(r.engine().view()) == balls,
          "resumed used-ball throw keeps id, seed and the spent ball");
    published(r, "ruins-resume-used-ball");
  }
}
}
