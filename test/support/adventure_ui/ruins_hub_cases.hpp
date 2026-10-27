#pragma once
#include "ruins_harness.hpp"

// Locked hub, clue entry, order puzzle and the first boss capture.
namespace {
void ruinsEntryAndClue() {
  MemoryStore store; TestPlatform p; Runtime r(store, p); start(r); skipDateAndStarter(r, p);
  openRuins(r, p); published(r, "ruins-locked");
  check(ruinsHeading("传说遗迹") && ruinsText("收集三条线索", 80, theme::kPurple, false) &&
        ruinsLine("线索 0 / 3", 196, theme::kText),
        "locked hub renders its heading, clue count and subtitle");
  check(!r.engine().view().ruins.unlocked, "fresh save enters an unsolved ruins hub");
  select(r, p, 0);
  check(r.screen().page == Page::RuinsClues, "hub opens the clue list");
  published(r, "ruins-clues");
  check(ruinsHeading("寻找线索") && ruinsLine("森林 · 胜利即可", 196, theme::kGold),
        "clue list names the selected route task");
  select(r, p, 0);
  check(r.screen().page == Page::Encounter &&
        r.engine().view().encounter.origin == EncounterOrigin::Ruins &&
        r.engine().view().encounter.species == 10, "clue battle uses the fixed Forest opponent");
  published(r, "ruins-clue-ready");
  select(r, p, 0);
  check(r.screen().page == Page::Battle, "clue battle starts from the real encounter page");
  published(r, "ruins-clue-battle");
  idle(r, 180); tap(r, p, 233, 30);
  for (unsigned i = 0; r.screen().page == Page::Battle && i < 150; ++i) frame(r);
  check(r.screen().page == Page::Pause, "clue battle pauses after the current cue");
  published(r, "ruins-clue-pause");
  select(r, p, 0);
  check(r.screen().page == Page::Battle, "pause resumes the clue battle");
  const uint64_t balls = availableBalls(r.engine().view());
  const uint64_t steps = r.engine().view().totalSteps;
  ruinsWin(r, p);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::RuinsClue,
        "clue victory lands on the clue result page");
  check(r.engine().view().ruins.clueMask == 1 && r.engine().view().encounter.usedBalls == 0,
        "clue victory awards the clue without a ball");
  check(availableBalls(r.engine().view()) == balls && r.engine().view().totalSteps == steps,
        "clue victory spends no ball and no step");
  published(r, "ruins-clue-result");
  check(ruinsHeading("线索已获得") && ruinsLine("第2位 · 线索 1 / 3", 168, theme::kText),
        "clue result shows the obtained clue, its slot and its count");
  select(r, p, 0);
  check(r.screen().page == Page::Ruins && r.engine().view().ruins.clueMask == 1,
        "clue result confirms back to the ruins hub");
  published(r, "ruins-clue-hub");
  idle(r, 30020);
  check(r.dimmed() && r.screen().page == Page::Ruins, "ruins hub enters power saving");
  edge(r, p, false, true);
  check(!r.dimmed() && r.screen().page == Page::Ruins, "wake press keeps the ruins hub");
  backToMenu(r, p);
}

void ruinsOrderAndCapture() {
  MemoryStore store; Save s;
  s.sequence = 6; s.bootGeneration = 1; s.starter = s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = 5000; s.explorationCredit = 5000;
  s.nextEncounterId = 2; s.activityEncounterCount = 1; s.ruins.clueMask = kRuinsAllClues;
  check(validate(s), "three-clue fixture validates before the puzzle");
  check(store.commit(s) == CommitStatus::Committed, "three-clue fixture encodes");
  TestPlatform p; Runtime r(store, p); start(r);
  openRuins(r, p);
  select(r, p, 1);
  check(r.screen().page == Page::RuinsOrder, "clue hub opens the route order puzzle");
  select(r, p, 0); select(r, p, 1); select(r, p, 2);
  check(r.screen().page == Page::RuinsOrder && r.screen().orderConflict == 0,
        "wrong order reports the first conflicting slot and stays on the puzzle");
  check(!r.engine().view().ruins.unlocked && r.engine().view().ruins.clueMask == kRuinsAllClues,
        "wrong order neither unlocks nor clears clues");
  published(r, "ruins-order-conflict");
  check(ruinsHeading("排列路线") && ruinsLine("第 1 位冲突", 188, theme::kRed),
        "order conflict marks the first slot on screen");
  select(r, p, 1); select(r, p, 0); select(r, p, 2);
  check(r.screen().page == Page::Ruins && r.engine().view().ruins.unlocked,
        "Coast, Forest, Cave unlocks the boss list exactly once");
  published(r, "ruins-unlocked");
  select(r, p, 0);
  check(r.screen().page == Page::RuinsTargets, "unlocked hub opens the eleven-target list");
  published(r, "ruins-targets");
  check(ruinsHeading("选择目标") && ruinsText("已捕获 0 / 11", 80, theme::kPurple, false),
        "target list shows its heading and captured count");
  // Walk to the last page without confirming the return row.
  for (unsigned guard = 0; r.screen().selected != 11 && guard < 40; ++guard)
    edge(r, p, false, true);
  check(r.screen().page == Page::RuinsTargets && r.screen().selected == 11,
        "target list reaches its last page");
  published(r, "ruins-targets-last");
  edge(r, p, true, false);
  check(r.screen().selected == 10, "last target is selectable on the last page");
  edge(r, p, true, true);
  check(r.screen().page == Page::RuinsBoss && r.engine().view().ruins.lastTarget == 251,
        "selecting a target opens its boss detail directly without starting a fight");
  published(r, "ruins-boss-251");
  check(ruinsHeading(speciesName(251)), "boss detail renders the catalog heading");
  ruinsBossSpec("生命 我方3 / 首领4", "节拍 1.4 秒 · 12 拍", "节奏 交替 · 判定 180/70ms");
  idle(r, 60); tap(r, p, 233, 30);
  check(r.screen().page == Page::Ruins, "boss detail back entry returns to the hub");
  select(r, p, 1);
  const uint64_t id = r.engine().view().nextEncounterId;
  p.seed = ruinsThrowSeed(id, 251, true);
  select(r, p, 0);
  check(r.screen().page == Page::Encounter && r.engine().view().encounter.species == 251,
        "starting the challenge creates the selected boss encounter");
  published(r, "ruins-boss-ready");
  select(r, p, 0);
  check(r.screen().page == Page::Battle && r.engine().battleView().enemyMaxHp == 4 &&
        r.engine().battleView().beatMs == 1400, "boss battle uses its catalog HP and beat");
  published(r, "ruins-boss-battle");
  check(ruinsHeading("节奏对战"), "boss battle keeps the shared battle heading");
  ruinsWin(r, p);
  check(r.screen().page == Page::Throw && r.engine().view().phase == Phase::ThrowReady,
        "boss win enters the existing throw flow");
  published(r, "ruins-boss-throw");
  edge(r, p, false, true);
  check(r.engine().throwView().active, "boss throw charges without consuming a ball");
  throwAt(r, p, r.engine().throwView().targetMs);
  for (unsigned i = 0; i < 400 && r.engine().view().phase == Phase::ThrowReady; ++i) frame(r);
  idle(r, 4000);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::Captured,
        "boss capture reuses the ordinary capture result");
  check(r.engine().view().captureCounts[250] == 1 && r.engine().view().ballsSpent == 1,
        "boss capture raises the counter and spends one ball");
  published(r, "ruins-boss-captured");
  select(r, p, 0);
  check(r.screen().page == Page::Ruins && r.engine().view().phase == Phase::Walking,
        "capture result confirms back to the ruins hub");
  published(r, "ruins-after-capture");
  // The hub summary is drawn above the three-row menu panel (top edge 208); the
  // whole target line has to stay readable, not just its top edge.
  check(ruinsLine("目标 时拉比", 172, theme::kText), "hub keeps the whole target name");
  check(ruinsLine("生命 4 · 节拍 1.40 秒 · 交替", 196, theme::kMuted),
        "hub keeps the whole boss summary above the menu panel");
  select(r, p, 0);
  published(r, "ruins-targets-completed");
  select(r, p, 11); select(r, p, 2);
  check(r.screen().page == Page::Menu, "return entry leaves the ruins from the target list");
}
}
