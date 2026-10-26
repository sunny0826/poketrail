#pragma once
#include "ruins_harness.hpp"

// Clue collection order and the marked positions shown in the UI.
namespace {
void ruinsCollectClue(Runtime& r, TestPlatform& p, unsigned index) {
  check(r.screen().page == Page::Ruins, "clue collection continues from the ruins hub");
  select(r, p, 0);
  check(r.screen().page == Page::RuinsClues, "hub opens the clue list for collection");
  select(r, p, index);
  check(r.screen().page == Page::Encounter &&
        r.engine().view().encounter.species == ruinsClueAt(uint8_t(index)).opponent,
        "clue entry starts its catalog opponent");
  select(r, p, 0);
  check(r.screen().page == Page::Battle, "clue battle starts from the real encounter page");
  ruinsWin(r, p);
  check(r.engine().view().result == Result::RuinsClue, "clue battle awards its clue");
  select(r, p, 0);
  check(r.screen().page == Page::Ruins, "clue result confirms back to the hub");
}
void ruinsClueOrderVisibility() {
  const uint8_t orders[3][3] = {{0, 1, 2}, {1, 0, 2}, {2, 0, 1}};
  for (unsigned order = 0; order < 3; ++order) {
    MemoryStore store; TestPlatform p; Runtime r(store, p); start(r); skipDateAndStarter(r, p);
    openRuins(r, p);
    for (unsigned slot = 0; slot < kRuinsClueTotal; ++slot)
      ruinsCollectClue(r, p, orders[order][slot]);
    check(r.engine().view().ruins.clueMask == kRuinsAllClues,
          "any collection order fills exactly the three clue bits");
    select(r, p, 0);
    check(r.screen().page == Page::RuinsClues, "clue list stays reviewable after all clues");
    published(r, "ruins-clue-positions");
    // Clue ids stay Forest/Coast/Cave; the announced positions follow the
    // answer order, so Coast is 1, Forest 2 and Cave 3 in every collection order.
    check(ruinsRowText("第2位 · 森林", 238, theme::kPurple),
          "forest clue row announces its position on the selected row");
    check(ruinsRowText("第1位 · 海边", 306, theme::kText),
          "coast clue row announces position 1");
    check(ruinsRowText("第3位 · 洞窟", 374, theme::kText),
          "cave clue row announces position 3");
    check(ruinsLine("第2位 · 森林 · 胜利即可", 196, theme::kMint),
          "selected clue detail repeats the marked position");
    select(r, p, 3);
    check(r.screen().page == Page::Ruins, "clue list back entry returns to the hub");
    select(r, p, 1);
    check(r.screen().page == Page::RuinsOrder, "hub opens the route order puzzle");
    check(ruinsText("按线索标出的顺序", 80, theme::kPurple, false),
          "puzzle asks for the order marked on the clues, not the collection order");
    select(r, p, 1); select(r, p, 0); select(r, p, 2);
    check(r.screen().page == Page::Ruins && r.engine().view().ruins.unlocked,
          "the marked Coast/Forest/Cave order unlocks after any collection order");
    published(r, "ruins-unlocked-after-order");
  }
}
}
