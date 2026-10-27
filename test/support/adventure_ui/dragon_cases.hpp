#pragma once
#include "harness.hpp"
#include "ruins_harness.hpp"
#include "dragon_trial_catalog.hpp"

// Dragon cave Runtime cases. They drive the real Runtime, real M5GFX frames and
// the same Engine commands the device uses; screenshots are only accepted after
// the whole frame stays inside the round screen.
namespace {
void dragonMilestonesUi(Save& s) {
  static const uint64_t life[] = {10000, 50000, 100000};
  for (unsigned index = 0; index < 3; ++index) {
    s.lifetime[index].reached = s.totalSteps >= life[index];
    if (s.lifetime[index].reached) {
      s.lifetime[index].first.bootGeneration = s.bootGeneration;
      s.lifetime[index].first.totalSteps = life[index];
    }
  }
}
Save dragonFixture(uint64_t captures, uint16_t capturedSpecies = 0, bool noBalls = false) {
  Save s;
  s.sequence = 4; s.bootGeneration = 1; s.starter = s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = captures * kEncounterSteps;
  s.nextEncounterId = captures + 1;
  if (captures) {
    const uint16_t species = capturedSpecies ? capturedSpecies : 1;
    setRegistered(s, species); setOwned(s, species);
    s.captureCounts[species - 1] = uint32_t(captures);
    Capture& card = s.firstCapture[species - 1];
    card.species = species; card.route = Route::Forest;
    card.at.bootGeneration = 1; card.at.totalSteps = 500;
  }
  if (noBalls) s.rewardBaseSteps = s.totalSteps;
  dragonMilestonesUi(s);
  check(validate(s), "dragon UI fixture validates");
  return s;
}
void dragonStore(MemoryStore& store, uint64_t captures, uint16_t capturedSpecies = 0,
                 bool noBalls = false) {
  check(store.commit(dragonFixture(captures, capturedSpecies, noBalls)) == CommitStatus::Committed,
        "dragon UI fixture encodes");
}
uint32_t dragonUiSeed(uint64_t id, uint16_t species, bool first) {
  for (uint32_t seed = 0; seed < 2000000; ++seed) {
    Encounter probe; probe.seed = seed; probe.id = id; probe.species = species;
    const uint8_t p = captureProbability(catalogEntry(species)->rare, Timing::Perfect);
    if ((throwRoll(probe, 1) < p) == first) return seed;
  }
  return 0;
}
// Trailing menu notes use middle_right at x = 377, unlike the centred oracles.
bool dragonTrailing(const char* text, int y, uint16_t color) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  if (!expected.createSprite(240, 40)) return false;
  expected.fillScreen(TFT_BLACK);
  expected.setFont(&fonts::efontCN_24);
  expected.setTextDatum(middle_right);
  expected.setTextColor(color);
  expected.drawString(text, 200, 20);
  bool same = false;
  for (int shift = -1; shift <= 1 && !same; ++shift) {
    same = true;
    for (int row = 2; row < 36 && same; ++row) for (int px = 20; px < 236; ++px) {
      const uint16_t pixel = expected.readPixel(px, row);
      if (pixel && M5.Display.readPixel(177 + px, y - 20 + row + shift) != pixel) {
        same = false; break;
      }
    }
  }
  expected.deleteSprite();
  return same;
}
void openDragon(Runtime& r, TestPlatform& p) {
  openMenu(r, p); select(r, p, 8);
  check(r.screen().page == Page::Dragon, "main menu opens the dragon cave entry");
}
void dragonBattleWin(Runtime& r, TestPlatform& p, uint8_t missDodges = 0) {
  uint8_t missed = 0;
  for (unsigned i = 0; r.engine().view().phase == Phase::Battle && i < 20; ++i) {
    const auto b = r.engine().battleView();
    while (int32_t(b.targetMs - host_time_ms) > 20) frame(r);
    if (b.expected == Action::Dodge && missed < missDodges) {
      ++missed; idle(r, b.normalWindowMs + 120); continue;
    }
    p.next.left = b.expected == Action::Dodge;
    p.next.right = b.expected == Action::Attack;
    p.next.held = true;
    frame(r, b.targetMs - host_time_ms); frame(r);
    if (r.engine().view().phase == Phase::Battle) idle(r, b.normalWindowMs + 120);
  }
  check(r.engine().view().phase != Phase::Battle, "dragon stage settles");
  idle(r, 700);
}
void dragonBattleLose(Runtime& r, TestPlatform& p) {
  for (unsigned i = 0; r.engine().view().phase == Phase::Battle && i < 20; ++i) {
    const auto b = r.engine().battleView();
    while (int32_t(b.targetMs - host_time_ms) > 20) frame(r);
    // Pressing the opposite action is always a miss and never advances the win.
    p.next.left = b.expected == Action::Attack;
    p.next.right = b.expected == Action::Dodge;
    p.next.held = true;
    frame(r, b.targetMs - host_time_ms); frame(r);
    if (r.engine().view().phase == Phase::Battle) idle(r, b.normalWindowMs + 120);
  }
  check(r.engine().view().phase == Phase::Resolved, "dragon defeat closes the stage");
  idle(r, 700);
}
void dragonThrow(Runtime& r, TestPlatform& p) {
  check(r.screen().page == Page::Throw, "final win opens the throw page");
  edge(r, p, false, true);
  check(r.engine().throwView().active, "throw page prepares one ball");
  throwAt(r, p, r.engine().throwView().targetMs);
  idle(r, 5000);
}

void dragonHubEligibility() {
  const uint64_t blocked[] = {0, 5, 7, 11};
  const char* note[] = {"还差 6 次", "还差 1 次", "还差 5 次", "还差 1 次"};
  const char* names[] = {"dragon-blocked-zero", "dragon-blocked-five",
                         "dragon-blocked-seven", "dragon-blocked-eleven"};
  for (unsigned index = 0; index < 4; ++index) {
    MemoryStore store; dragonStore(store, blocked[index]);
    TestPlatform p; Runtime r(store, p); start(r);
    check(r.screen().page == Page::Home, "blocked fixture starts at exploration");
    openDragon(r, p);
    check(menuCount(r.screen(), r.engine()) == 3, "dragon hub lists two trials and back");
    check(dragonTrailing(note[index], 238, theme::kPurple), "hub row states the remaining captures");
    const auto before = r.engine().view();
    select(r, p, 0);
    check(r.screen().page == Page::Dragon && r.engine().view().phase == Phase::Walking,
          "unqualified trial cannot start");
    check(r.engine().view().encounter.id == before.encounter.id, "rejected start allocates no ID");
    published(r, names[index]);
  }
  for (uint64_t captures : {uint64_t(6), uint64_t(12)}) {
    MemoryStore store; dragonStore(store, captures);
    TestPlatform p; Runtime r(store, p); start(r);
    openDragon(r, p);
    check(ruinsText("捕获 6 的倍数 · 本轮开放", 80, theme::kPurple, false),
          "hub states the trial window is open");
    char total[32]; std::snprintf(total, sizeof(total), "已捕获 %llu 次",
                                  static_cast<unsigned long long>(captures));
    check(ruinsText(total, 124, theme::kMuted, false), "hub shows the unified capture total");
    check(dragonTrailing("可挑战", 238, theme::kPurple), "first trial row is challengeable");
    char name[32]; std::snprintf(name, sizeof(name), "dragon-hub-%llu",
                                 static_cast<unsigned long long>(captures));
    published(r, name);
    // Both trials are independently selectable at this window.
    select(r, p, 0);
    check(r.screen().page == Page::DragonStage &&
          r.engine().view().dragonTrial.trialId == 1, "first trial starts");
    published(r, "dragon-stage-dratini");
    // Retreat closes only this run; the window stays open for the second trial.
    select(r, p, 0);
    tap(r, p, 233, 30); idle(r, 1800);
    check(r.screen().page == Page::Pause, "dragon retreat is reachable from the pause page");
    select(r, p, 1);
    check(r.screen().page == Page::Result && r.engine().view().result == Result::Retreated,
          "retreat reaches the result page");
    select(r, p, 1);
    check(r.screen().page == Page::Dragon && !r.engine().view().dragonTrial.active,
          "returning from a retreat abandons the run");
    select(r, p, 1);
    check(r.screen().page == Page::DragonStage &&
          r.engine().view().dragonTrial.trialId == 2, "second trial starts independently");
    published(r, "dragon-stage-larvitar");
  }
}

void dragonDratiniFlow() {
  MemoryStore store; dragonStore(store, 6);
  TestPlatform p; Runtime r(store, p); start(r);
  // The trial activity owns the next ID; find a seed whose first ball captures.
  p.seed = dragonUiSeed(7, 147, true);
  openDragon(r, p); select(r, p, 0);
  check(r.screen().page == Page::DragonStage, "trial opens the stage sheet");
  check(r.engine().view().dragonTrial.stageIndex == 0 &&
        r.engine().view().dragonTrial.stageStartHp == 3, "stage one snapshot");
  check(ruinsLine("第 1 / 2 阶段", 80, theme::kPurple, false), "stage sheet names the stage");
  published(r, "dragon-stage-dratini");
  select(r, p, 0);
  check(r.screen().page == Page::Battle, "stage start enters the real battle");
  const auto& b = r.engine().battleView();
  check(b.enemyHp == 3 && b.beatMs == 1500 && b.normalWindowMs == 180 &&
        b.perfectWindowMs == 70, "stage one uses the catalog spec");
  published(r, "dragon-battle-stage1");
  tap(r, p, 233, 30); idle(r, 1800);
  check(r.screen().page == Page::Pause, "dragon battle pauses after the cue");
  published(r, "dragon-pause");
  select(r, p, 0);
  check(r.screen().page == Page::Battle, "dragon battle resumes");
  dragonBattleWin(r, p);
  check(r.screen().page == Page::DragonUpgrade && r.engine().view().result == Result::DragonUpgrade,
        "non-final win opens the upgrade page");
  check(menuCount(r.screen(), r.engine()) == 3, "upgrade page offers three rows");
  check(dragonTrailing("已满", 238, theme::kMuted), "full HP disables the heal row");
  published(r, "dragon-upgrade-full");
  select(r, p, 1);
  check(r.screen().page == Page::DragonStage, "window upgrade advances to the next stage");
  check(r.engine().view().dragonTrial.normalBonus == 30 &&
        r.engine().view().dragonTrial.perfectBonus == 10, "window bonus persists");
  check(ruinsLine("窗口 210/80ms", 194, theme::kMuted, false), "stage sheet shows the real windows");
  published(r, "dragon-stage2-upgraded");
  select(r, p, 0);
  check(r.engine().battleView().normalWindowMs == 210 &&
        r.engine().battleView().perfectWindowMs == 80 &&
        r.engine().battleView().enemyHp == 4, "battle uses the upgraded timeline");
  published(r, "dragon-battle-upgraded");
  dragonBattleWin(r, p);
  check(r.screen().page == Page::Throw, "final win opens the throw page");
  published(r, "dragon-throw-ready");
  dragonThrow(r, p);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::Captured,
        "ball capture reaches the result card");
  check(r.engine().view().captureCounts[146] == 1, "captured Dratini joins the unified account");
  published(r, "dragon-captured");
  captureCardBadges(r, 8);
  select(r, p, 0);
  check(r.screen().page == Page::Dragon && !r.engine().view().dragonTrial.active,
        "confirm returns to the hub and closes the trial");
  check(dragonTrailing("还差 5 次", 306, theme::kText),
        "seven captures close the other trial until the next multiple");
  published(r, "dragon-hub-after-capture");
}

void dragonLarvitarFlow() {
  MemoryStore store; dragonStore(store, 6);
  TestPlatform p; Runtime r(store, p); start(r);
  // The trial owns ID 7; pick a seed whose first ball captures Larvitar.
  p.seed = dragonUiSeed(7, 246, true);
  openDragon(r, p); select(r, p, 1);
  select(r, p, 0);
  dragonBattleWin(r, p, 1);  // one missed dodge: carry two HP
  check(r.screen().page == Page::DragonUpgrade, "three-stage trial asks for an upgrade");
  check(r.engine().view().dragonTrial.stageStartHp == 2, "carried HP is saved");
  check(dragonTrailing("可选", 238, theme::kGold), "heal is offered while injured");
  published(r, "dragon-upgrade-hurt");
  select(r, p, 0);
  check(r.engine().view().dragonTrial.stageStartHp == 3, "heal restores one HP");
  select(r, p, 0);
  dragonBattleWin(r, p);
  check(r.screen().page == Page::DragonUpgrade, "second non-final win asks again");
  check(dragonTrailing("可选", 374, theme::kText), "guard is offered once");
  published(r, "dragon-upgrade-guard");
  select(r, p, 2);
  check(r.engine().view().dragonTrial.guard == 1, "guard upgrade is saved");
  select(r, p, 0);
  check(r.engine().battleView().enemyHp == 5 && r.engine().battleView().beatMs == 1350 &&
        r.engine().battleView().guard == 1, "final stage uses five HP, 1350ms and the guard");
  published(r, "dragon-battle-final");
  dragonBattleWin(r, p, 1);  // first missed dodge is absorbed
  check(r.engine().view().phase == Phase::ThrowReady, "guarded final stage still wins");
  published(r, "dragon-throw-larvitar");
  dragonThrow(r, p);
  check(r.screen().page == Page::Result && r.engine().view().result == Result::Captured,
        "Larvitar reaches the shared capture result");
  check(r.engine().view().captureCounts[245] == 1 && r.engine().view().ballsSpent == 1,
        "captured Larvitar joins the unified account with one spent ball");
  published(r, "dragon-captured-larvitar");
  captureCardBadges(r, 8);
}

void dragonCoreCases() {
  dragonHubEligibility();
  dragonDratiniFlow();
  dragonLarvitarFlow();
}
}
