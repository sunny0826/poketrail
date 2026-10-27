#pragma once
#include "adventure.hpp"
#include "save_codec.hpp"
#include "encounter_catalog.hpp"
#include "ruins_catalog.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace pokedex::adventure;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static std::vector<uint8_t> encode(const Save& s) {
  std::vector<uint8_t> data(codec::maxEncodedSize); size_t n = 0;
  CHECK(codec::encode(s, data.data(), data.size(), n)); data.resize(n); return data;
}
static void checksum(std::vector<uint8_t>& data) {
  std::vector<uint8_t> checked(data.begin(), data.begin() + 18);
  checked.insert(checked.end(), data.begin() + 22, data.end());
  const uint32_t crc = codec::crc32(checked.data(), checked.size());
  for (unsigned i = 0; i < 4; ++i) data[18 + i] = uint8_t(crc >> (8 * i));
}
static size_t firstPayloadDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  CHECK(a.size() == b.size());
  for (size_t i = 22; i < a.size(); ++i) if (a[i] != b[i]) return i;
  CHECK(false); return 0;
}
static void reject(const Save& s) {
  CHECK(!validate(s)); std::vector<uint8_t> out(codec::maxEncodedSize); size_t n = 123;
  CHECK(!codec::encode(s, out.data(), out.size(), n) && n == 0);
}
static void patchPayload(std::vector<uint8_t>& data) {
  const uint32_t payload = uint32_t(data.size() - codec::kHeaderSize);
  for (unsigned i = 0; i < 4; ++i) data[14 + i] = uint8_t(payload >> (8 * i));
  checksum(data);
}
// One ordinary encounter that charged 5000 steps, without any activity history.
static Save ordinaryModern() {
  Save s; s.sequence = 9; s.bootGeneration = 1; s.starter = s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = 5000; s.nextEncounterId = 2;
  s.encounter.id = 1; s.encounter.species = 1; s.encounter.opponent = 1;
  s.encounter.partner = 1; s.encounter.route = Route::Forest;
  s.encounter.ruleVersion = 2; s.phase = Phase::EncounterReady;
  return s;
}
// An unlocked ruins boss encounter issued without spending the 5000-step
// ordinary credit. clueMask/unlocked/lastTarget now follow the fixed catalog.
static Save activityModern() {
  Save s = ordinaryModern();
  s.activityEncounterCount = 1; s.explorationCredit = 5000;
  s.encounter.species = 251; s.encounter.opponent = 251;
  s.encounter.origin = EncounterOrigin::Ruins;
  s.ruins.clueMask = kRuinsAllClues; s.ruins.unlocked = true; s.ruins.lastTarget = 251;
  return s;
}
// A dragon trial stage issued on the same activity ledger. The snapshot points
// at the stage that follows one chosen window upgrade, so it stays inside the
// one-choice-per-stage budget.
static Save dragonModern() {
  Save s = ordinaryModern();
  s.activityEncounterCount = 1; s.explorationCredit = 5000;
  s.encounter.species = 147; s.encounter.opponent = 147;
  s.encounter.origin = EncounterOrigin::DragonTrial;
  s.dragonTrial.active = true; s.dragonTrial.trialId = 1;
  s.dragonTrial.stageIndex = 1; s.dragonTrial.stageStartHp = 2;
  s.dragonTrial.normalBonus = 30; s.dragonTrial.perfectBonus = 10;
  s.dragonTrial.guard = 0;
  return s;
}
// The same trial waiting for the second upgrade choice: guard was bought after
// stage 0, and the full-HP stage 1 win leaves only the window row selectable.
static Save dragonUpgradeModern() {
  Save s = dragonModern();
  s.encounter.species = 246; s.encounter.opponent = 246;
  s.dragonTrial.trialId = 2; s.dragonTrial.stageIndex = 2;
  s.dragonTrial.stageStartHp = 3;
  s.dragonTrial.normalBonus = 0; s.dragonTrial.perfectBonus = 0;
  s.dragonTrial.guard = 1;
  s.phase = Phase::Resolved; s.result = Result::DragonUpgrade;
  return s;
}
