#pragma once
#include "dragon_cases.hpp"

// Upgrade budget, real per-stage waits and forged-save rejection cases. They
// reuse the fixtures, drivers and real catalog from dragon_cases.hpp.
// A real command snapshot must survive the production codec unchanged; this is
// the positive half of every budget case below.
static void dragonRoundTrip(const Save& s) {
  CHECK(validate(s));
  std::vector<uint8_t> out(codec::maxEncodedSize); size_t n = 0;
  CHECK(codec::encode(s, out.data(), out.size(), n) && n);
  Save back;
  CHECK(codec::decode(out.data(), n, back) == codec::DecodeStatus::Ok);
  CHECK(validate(back));
}
// Rewrite the schema-3 activity block inside already-encoded bytes and refresh
// the CRC, so decode rejects on semantics instead of framing.
static std::vector<uint8_t> dragonForge(const Save& legal, uint8_t normalBonus,
                                        uint8_t perfectBonus, uint8_t hp, uint8_t guard) {
  std::vector<uint8_t> data(codec::maxEncodedSize); size_t n = 0;
  CHECK(codec::encode(legal, data.data(), data.size(), n));
  data.resize(n);
  const size_t block = data.size() - 22;
  data[block + 18] = hp;
  data[block + 19] = normalBonus;
  data[block + 20] = perfectBonus;
  data[block + 21] = guard;
  std::vector<uint8_t> checked(data.begin(), data.begin() + 18);
  checked.insert(checked.end(), data.begin() + 22, data.end());
  const uint32_t crc = codec::crc32(checked.data(), checked.size());
  for (unsigned i = 0; i < 4; ++i) data[18 + i] = uint8_t(crc >> (8 * i));
  return data;
}

static void dragonUpgradeRules() {
  // Stage 0 win at full HP: only the window and guard rows are available.
  DragonWorld full(6);
  CHECK(full.e.beginDragonTrial(2, 77) == CommandResult::Applied);
  CHECK(dragonPlay(full) == Result::DragonUpgrade);
  const Save waiting1 = full.e.view();
  CHECK(waiting1.phase == Phase::Resolved && waiting1.result == Result::DragonUpgrade);
  CHECK(waiting1.dragonTrial.stageIndex == 1 && waiting1.dragonTrial.stageStartHp == 3);
  CHECK(dragonUpgradeCount(waiting1.dragonTrial) == 0 &&
        dragonUpgradeBudgetLegal(waiting1.dragonTrial, true));
  dragonRoundTrip(waiting1);
  CHECK(full.e.chooseDragonUpgrade(0) == CommandResult::InvalidInput);  // already full
  CHECK(full.e.chooseDragonUpgrade(3) == CommandResult::InvalidInput);
  CHECK(full.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  const Save upgraded1 = full.e.view();
  CHECK(upgraded1.phase == Phase::EncounterReady);
  CHECK(upgraded1.dragonTrial.normalBonus == 30 && upgraded1.dragonTrial.perfectBonus == 10);
  CHECK(upgraded1.dragonTrial.stageIndex == 1 &&
        dragonUpgradeCount(upgraded1.dragonTrial) == 1 &&
        dragonUpgradeBudgetLegal(upgraded1.dragonTrial, false));
  dragonRoundTrip(upgraded1);
  CHECK(full.e.chooseDragonUpgrade(1) == CommandResult::InvalidPhase);  // no repeat
  CHECK(full.e.startBattle(full.now) == CommandResult::Applied);
  CHECK(full.e.battleView().enemyHp == 4 && full.e.battleView().normalWindowMs == 210 &&
        full.e.battleView().perfectWindowMs == 80);

  // A hurt stage-0 win can buy the heal; the heal is not cumulative, so the
  // saved window/guard count stays below the stage budget.
  DragonWorld heal(6);
  CHECK(heal.e.beginDragonTrial(2, 77) == CommandResult::Applied);
  CHECK(dragonPlay(heal, true, 1) == Result::DragonUpgrade);
  CHECK(heal.e.view().dragonTrial.stageStartHp == 2);
  CHECK(heal.e.chooseDragonUpgrade(0) == CommandResult::Applied);
  CHECK(heal.e.view().dragonTrial.stageStartHp == 3 &&
        dragonUpgradeCount(heal.e.view().dragonTrial) == 0 &&
        heal.e.view().dragonTrial.stageIndex == 1);
  dragonRoundTrip(heal.e.view());

  // Guard first, then a full-HP stage 1 win: the second wait keeps the window
  // row selectable while the heal and guard rows are dead.
  DragonWorld guarded(6);
  CHECK(guarded.e.beginDragonTrial(2, 77) == CommandResult::Applied);
  CHECK(dragonPlay(guarded) == Result::DragonUpgrade);
  CHECK(guarded.e.chooseDragonUpgrade(2) == CommandResult::Applied);
  CHECK(guarded.e.view().dragonTrial.guard == 1);
  dragonRoundTrip(guarded.e.view());
  CHECK(dragonPlay(guarded) == Result::DragonUpgrade);
  const Save waiting2 = guarded.e.view();
  CHECK(waiting2.dragonTrial.stageIndex == 2 && waiting2.dragonTrial.stageStartHp == 3 &&
        waiting2.dragonTrial.guard == 1 && waiting2.dragonTrial.normalBonus == 0);
  CHECK(dragonUpgradeCount(waiting2.dragonTrial) == 1 &&
        dragonUpgradeBudgetLegal(waiting2.dragonTrial, true));
  CHECK(!dragonHealAvailable(waiting2.dragonTrial.stageStartHp));
  CHECK(!dragonGuardAvailable(waiting2.dragonTrial));
  CHECK(dragonWindowUpgradeAvailable(waiting2.dragonTrial));
  CHECK(dragonAnyUpgradeAvailable(waiting2.dragonTrial, waiting2.dragonTrial.stageStartHp));
  dragonRoundTrip(waiting2);
  CHECK(guarded.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  const Save upgraded2 = guarded.e.view();
  CHECK(upgraded2.dragonTrial.stageIndex == 2 && upgraded2.dragonTrial.normalBonus == 30 &&
        upgraded2.dragonTrial.perfectBonus == 10 && upgraded2.dragonTrial.guard == 1);
  CHECK(dragonUpgradeCount(upgraded2.dragonTrial) == 2 &&
        dragonUpgradeBudgetLegal(upgraded2.dragonTrial, false));
  dragonRoundTrip(upgraded2);
  CHECK(guarded.e.startBattle(guarded.now) == CommandResult::Applied);
  CHECK(guarded.e.battleView().normalWindowMs == 210 &&
        guarded.e.battleView().perfectWindowMs == 80 &&
        guarded.e.battleView().guard == 1);
  CHECK(dragonFinish(guarded) == Result::Won);
  dragonRoundTrip(guarded.e.view());
  CHECK(guarded.e.view().phase == Phase::ThrowReady &&
        guarded.e.view().dragonTrial.stageIndex == 2);

  // The window cap itself still yields the documented windows, but no saved
  // snapshot may accumulate six window upgrades plus the guard.
  DragonTrialState maxed;
  maxed.trialId = 2; maxed.stageIndex = 1; maxed.stageStartHp = 3;
  maxed.normalBonus = 180; maxed.perfectBonus = 60; maxed.guard = 1;
  CHECK(dragonUpgradeCount(maxed) == 7 && !dragonUpgradeBudgetLegal(maxed, true) &&
        !dragonUpgradeBudgetLegal(maxed, false));
  CHECK(!dragonHealAvailable(maxed.stageStartHp) && !dragonWindowUpgradeAvailable(maxed) &&
        !dragonGuardAvailable(maxed));
  CHECK(!dragonAnyUpgradeAvailable(maxed, maxed.stageStartHp));
  CHECK(dragonNormalWindow(maxed.normalBonus) == 360 &&
        dragonPerfectWindow(maxed.perfectBonus) == 130);
  std::puts("PASS dragon upgrades: real waits per stage, heals, guard, caps and budget");
}

static void dragonUpgradeRejections() {
  // Real waiting and post-upgrade snapshots supply the legal bases; every case
  // below either exceeds the one-choice-per-stage budget or rewrites stage 0.
  DragonWorld f(6);
  CHECK(f.e.beginDragonTrial(2, 77) == CommandResult::Applied);
  const Save stage0 = f.e.view();
  CHECK(stage0.dragonTrial.stageIndex == 0 && validate(stage0));
  CHECK(dragonPlay(f) == Result::DragonUpgrade);
  const Save waiting1 = f.e.view();
  CHECK(f.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  const Save upgraded1 = f.e.view();
  CHECK(f.e.startBattle(f.now) == CommandResult::Applied);
  CHECK(dragonFinish(f) == Result::DragonUpgrade);
  const Save waiting2 = f.e.view();
  CHECK(waiting2.dragonTrial.stageIndex == 2 &&
        dragonUpgradeCount(waiting2.dragonTrial) == 1);
  CHECK(f.e.chooseDragonUpgrade(1) == CommandResult::Applied);
  const Save upgraded2 = f.e.view();
  CHECK(upgraded2.dragonTrial.stageIndex == 2 &&
        dragonUpgradeCount(upgraded2.dragonTrial) == 2);

  auto reject = [](const Save& s) {
    CHECK(!validate(s));
    std::vector<uint8_t> data(codec::maxEncodedSize); size_t written = 99;
    CHECK(!codec::encode(s, data.data(), data.size(), written) && written == 0);
  };
  Save bad = stage0; bad.dragonTrial.stageStartHp = 2; reject(bad);
  bad = stage0; bad.dragonTrial.normalBonus = 30; bad.dragonTrial.perfectBonus = 10; reject(bad);
  bad = stage0; bad.dragonTrial.guard = 1; reject(bad);
  // A first wait has spent no choice yet.
  bad = waiting1; bad.dragonTrial.normalBonus = 30; bad.dragonTrial.perfectBonus = 10; reject(bad);
  bad = waiting1; bad.dragonTrial.guard = 1; reject(bad);
  // One completed stage cannot own two cumulative upgrades.
  bad = upgraded1; bad.dragonTrial.normalBonus = 60; bad.dragonTrial.perfectBonus = 20; reject(bad);
  bad = upgraded1; bad.dragonTrial.guard = 1; reject(bad);
  // The second wait can keep one upgrade, never two.
  dragonRoundTrip(waiting2);
  bad = waiting2; bad.dragonTrial.normalBonus = 60; bad.dragonTrial.perfectBonus = 20; reject(bad);
  bad = waiting2; bad.dragonTrial.guard = 1; reject(bad);
  // The final-stage snapshot can keep two upgrades, never three.
  bad = upgraded2; bad.dragonTrial.normalBonus = 90; bad.dragonTrial.perfectBonus = 30; reject(bad);

  // The R2 review fixture: full HP, maxed windows and the guard taken, waiting
  // for an impossible third choice. It is now rejected by validation, by
  // encode, by a CRC-corrected decode and by Engine::begin.
  const std::vector<uint8_t> forged = dragonForge(waiting1, 180, 60, 3, 1);
  Save out;
  CHECK(codec::decode(forged.data(), forged.size(), out) == codec::DecodeStatus::Corrupt);
  MemoryStore store; store.disk = forged;
  Engine boot(store);
  CHECK(boot.begin() == CommandResult::StorageError && !boot.ready());
  std::puts("PASS dragon upgrade snapshots reject over-budget, stage-0 and forged waits");
}

static void dragonUpgradeCases() {
  dragonUpgradeRules();
  dragonUpgradeRejections();
}
