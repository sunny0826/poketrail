#pragma once
#include "harness.hpp"

// Activity ledger, optional schema-3 block and illegal activity fields.
static void activityState() {
  CHECK(validate(ordinaryModern()));
  const std::vector<uint8_t> plainBytes = encode(ordinaryModern());
  CHECK(plainBytes.size() == 21520);
  CHECK(encode(ordinaryModern()).back() == 0);  // trailing activity block is all defaults
  Save base = activityModern();
  CHECK(validate(base));
  const std::vector<uint8_t> encodedBase = encode(base);
  CHECK(encodedBase.size() == 21520);
  Save back;
  CHECK(codec::decode(encodedBase.data(), encodedBase.size(), back) == codec::DecodeStatus::Ok);
  CHECK(back.activityEncounterCount == 1 && back.encounter.origin == EncounterOrigin::Ruins &&
        back.encounter.opponent == 251 && back.encounter.species == 251);
  CHECK(back.ruins.clueMask == kRuinsAllClues && back.ruins.unlocked && back.ruins.lastTarget == 251);
  CHECK(back.dragonTrial.active && back.dragonTrial.trialId == 1 &&
        back.dragonTrial.stageIndex == 0 && back.dragonTrial.stageStartHp == 3 &&
        back.dragonTrial.normalBonus == 20 && back.dragonTrial.perfectBonus == 10 &&
        back.dragonTrial.guard == 1);
  CHECK(encode(back) == encodedBase);

  // Activity IDs share the encounter sequence but do not consume exploration credit.
  Save mixed = base; mixed.nextEncounterId = 4; mixed.encounter.id = 3;
  mixed.activityEncounterCount = 2; mixed.explorationCredit = 0;
  CHECK(validate(mixed));
  Save allActivity = mixed; allActivity.activityEncounterCount = 3; allActivity.explorationCredit = 5000;
  CHECK(validate(allActivity));
  Save overIssued = mixed; overIssued.activityEncounterCount = 4; reject(overIssued);
  Save wrongCredit = mixed; wrongCredit.explorationCredit = 5000; reject(wrongCredit);
  Save noHistory = base; noHistory.activityEncounterCount = 0; reject(noHistory);

  // A current rule-2 ordinary encounter must own at least one ordinary ID, even
  // when the counts still satisfy the credit equation.
  Save unpaidOrdinary = ordinaryModern(); unpaidOrdinary.activityEncounterCount = 1;
  unpaidOrdinary.explorationCredit = 5000;
  reject(unpaidOrdinary);
  // The same books with a legacy-charged boundary stay legal.
  Save legacyMix; legacyMix.sequence = 9; legacyMix.bootGeneration = 1;
  legacyMix.starter = legacyMix.partner = 1; legacyMix.rosterRevision = 1;
  setRegistered(legacyMix, 1); setOwned(legacyMix, 1);
  legacyMix.totalSteps = legacyMix.undatedSteps = 5500;
  legacyMix.nextEncounterId = 3; legacyMix.legacyChargedCount = 1;
  legacyMix.encounter.id = 2; legacyMix.encounter.species = 1; legacyMix.encounter.opponent = 1;
  legacyMix.encounter.partner = 1; legacyMix.encounter.route = Route::Forest;
  legacyMix.encounter.ruleVersion = 2; legacyMix.phase = Phase::EncounterReady;
  legacyMix.explorationCredit = 0;
  CHECK(validate(legacyMix));
  Save unpaidLegacy = legacyMix; unpaidLegacy.activityEncounterCount = 1;
  unpaidLegacy.explorationCredit = 5000;
  reject(unpaidLegacy);
  // An all-activity tail still needs no ordinary credit. The current encounter
  // is a fixed Forest clue battle (opponent 10) issued by the ruins catalog.
  Save allActivityLegacy = unpaidLegacy;
  allActivityLegacy.encounter.origin = EncounterOrigin::Ruins;
  allActivityLegacy.encounter.species = allActivityLegacy.encounter.opponent = 10;
  CHECK(validate(allActivityLegacy));
  // A voided legacy boundary keeps the same rule for the current ordinary field.
  Save regionMix = legacyMix;
  regionMix.legacyChargedCount = 0; regionMix.voidedLegacyCount = 1;
  regionMix.activityEncounterCount = 1; regionMix.explorationCredit = 5500;
  reject(regionMix);
  Save regionActivity = regionMix; regionActivity.encounter.origin = EncounterOrigin::Ruins;
  regionActivity.encounter.species = regionActivity.encounter.opponent = 10;
  CHECK(validate(regionActivity));

  // An old schema-3 payload ends before the activity block and keeps defaults.
  std::vector<uint8_t> oldPayload = plainBytes;
  oldPayload.resize(oldPayload.size() - 22);
  CHECK(oldPayload.size() == 21498);
  patchPayload(oldPayload);
  Save oldBack;
  CHECK(codec::decode(oldPayload.data(), oldPayload.size(), oldBack) == codec::DecodeStatus::Ok);
  CHECK(oldBack.activityEncounterCount == 0 && oldBack.encounter.origin == EncounterOrigin::Ordinary &&
        oldBack.encounter.opponent == 0 && !oldBack.ruins.clueMask && !oldBack.ruins.unlocked &&
        !oldBack.ruins.lastTarget && !oldBack.dragonTrial.active);
  CHECK(encode(oldBack).size() == 21520);
  for (size_t cut = 1; cut < 22; ++cut) {
    std::vector<uint8_t> partial = oldPayload;
    partial.insert(partial.end(), plainBytes.end() - 22, plainBytes.end() - 22 + cut);
    patchPayload(partial);
    Save truncated;
    CHECK(codec::decode(partial.data(), partial.size(), truncated) == codec::DecodeStatus::Corrupt);
  }

  // Forged block fields are rejected by semantic validation, not only framing.
  const size_t block = encodedBase.size() - 22;
  std::vector<uint8_t> forged = encodedBase; forged[block + 8] = 3; checksum(forged);
  Save out; CHECK(codec::decode(forged.data(), forged.size(), out) == codec::DecodeStatus::Corrupt);
  forged = encodedBase; forged[block + 21] = 2; checksum(forged);
  CHECK(codec::decode(forged.data(), forged.size(), out) == codec::DecodeStatus::Corrupt);
  forged = encodedBase; forged[block] = 2; checksum(forged);
  CHECK(codec::decode(forged.data(), forged.size(), out) == codec::DecodeStatus::Corrupt);

  Save bad = base; bad.encounter.origin = EncounterOrigin(3); reject(bad);
  bad = base; bad.ruins.clueMask = 8; reject(bad);
  bad = base; bad.ruins.lastTarget = 252; reject(bad);
  bad = base; bad.dragonTrial.trialId = 3; reject(bad);
  bad = base; bad.dragonTrial.stageIndex = 8; reject(bad);
  bad = base; bad.dragonTrial.stageStartHp = 0; reject(bad);
  bad = base; bad.dragonTrial.stageStartHp = 10; reject(bad);
  bad = base; bad.dragonTrial.normalBonus = 181; reject(bad);
  bad = base; bad.dragonTrial.perfectBonus = 181; reject(bad);
  bad = base; bad.dragonTrial.guard = 2; reject(bad);
  bad = base; bad.dragonTrial.active = false; reject(bad);  // leftover trial fields
  // Ruins origin must match the fixed catalog: species/route/opponent, target
  // selection, unlock and the reserved-but-unimplemented dragon origin.
  Save clueRoute = base;
  clueRoute.encounter.species = clueRoute.encounter.opponent = 10;
  clueRoute.encounter.route = Route::Coast; reject(clueRoute);
  Save clueSpecies = base;
  clueSpecies.encounter.species = 10; clueSpecies.encounter.opponent = 54;
  clueSpecies.encounter.route = Route::Coast; reject(clueSpecies);
  Save lockedBoss = base; lockedBoss.ruins.unlocked = false; reject(lockedBoss);
  Save wrongSlot = base; wrongSlot.ruins.lastTarget = 150; reject(wrongSlot);
  Save otherTarget = base;
  otherTarget.encounter.species = otherTarget.encounter.opponent = 144; reject(otherTarget);
  Save dragonOrigin = base; dragonOrigin.encounter.origin = EncounterOrigin::DragonTrial; reject(dragonOrigin);
  bad = ordinaryModern(); bad.ruins.clueMask = 1; reject(bad);  // no activity history
  Save two = ordinaryModern(); two.schema = 2; two.encounter.opponent = 0;
  CHECK(validate(two));
  two.activityEncounterCount = 1; reject(two);
  Save twoOrigin = ordinaryModern(); twoOrigin.schema = 2; twoOrigin.encounter.opponent = 0;
  twoOrigin.encounter.origin = EncounterOrigin::Ruins; reject(twoOrigin);
  std::puts("PASS activity ledger, optional schema-3 block, old payload defaults, truncation and illegal fields");
}
