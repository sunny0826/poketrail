#pragma once
#include "harness.hpp"

// Schema 1/2/3 payload sizes, catalog wiring and probability table.
static void schemaChecks(Save& s, std::vector<uint8_t>& data, Save& decoded) {
  s.schema = 1; s.encounter.ruleVersion = 1;
  for (auto& a : s.encounter.attempts) a.ruleVersion = 1;
  s.starter = s.partner = 1; s.totalSteps = 100000; s.undatedSteps = 100000;
  s.nextEncounterId = 152; s.explorationCredit = 24500;
  for (size_t i = 0; i < 151; ++i) {
    setOwned(s, uint16_t(i + 1)); s.captureCounts[i] = 1;
    s.firstCapture[i].species = uint16_t(i + 1);
    s.firstCapture[i].at.bootGeneration = 1; s.firstCapture[i].at.totalSteps = 100000;
  }
  for (size_t i = 0; i < 3; ++i) {
    s.lifetime[i].reached = true; s.lifetime[i].first.totalSteps = 100000; s.lifetime[i].first.bootGeneration = 1;
  }
  s.recentSize = 32;
  for (size_t i = 0; i < 32; ++i) s.recent[i] = s.firstCapture[119 + i];
  CHECK(validate(s)); data = encode(s);
  CHECK(data.size() == 12139 && data.size() <= codec::maxEncodedSize);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(encode(decoded) == data && decoded.firstCapture[150].species == 151);
  CHECK(data.size() == 12139 && codec::crc32(data.data(), data.size()) == 0x648b7655u);
  Save two = s; two.schema = 2;
  two.registered = two.owned;
  two.legacyChargedCount = two.nextEncounterId - 1; two.voidedLegacyCount = 0;
  two.rewardBaseSteps = 0; two.ballsSpent = 0; two.rosterRevision = 1; two.lastEvolution = EvolutionReceipt();
  two.encounter = Encounter();
  CHECK(validate(two)); const auto twoBytes = encode(two);
  CHECK(twoBytes.size() == 14672 && twoBytes.size() < codec::maxEncodedSize);
  Save reloaded; CHECK(codec::decode(twoBytes.data(), twoBytes.size(), reloaded) == codec::DecodeStatus::Ok);
  CHECK(reloaded.schema == 2 && encode(reloaded) == twoBytes);
  Save wide; wide.sequence = 0x1020304050607080ULL; wide.bootGeneration = 1;
  wide.starter = 1; wide.partner = 251; setRegistered(wide, 1); setRegistered(wide, 251); setOwned(wide, 251);
  wide.rosterRevision = 1; wide.totalSteps = wide.undatedSteps = 5000; wide.nextEncounterId = 2;
  wide.explorationCredit = 0; wide.captureCounts[250] = 1;
  Capture last; last.species = 251; last.at.bootGeneration = 1; last.at.totalSteps = 5000;
  wide.firstCapture[250] = last; wide.recent[0] = last; wide.recentSize = 1;
  CHECK(validate(wide)); const auto wideBytes = encode(wide);
  CHECK(wideBytes.size() == 21520 && wideBytes.size() <= codec::maxEncodedSize);
  Save wideBack; CHECK(codec::decode(wideBytes.data(), wideBytes.size(), wideBack) == codec::DecodeStatus::Ok);
  CHECK(encode(wideBack) == wideBytes && wideBack.captureCounts[250] == 1 && wideBack.registered[31] == 0x04);
  Save full; full.sequence = 1; full.bootGeneration = 1; full.starter = full.partner = 1;
  full.rosterRevision = 1; full.totalSteps = full.undatedSteps = 251 * kEncounterSteps;
  full.nextEncounterId = 252; full.explorationCredit = 0;
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    setRegistered(full, id); setOwned(full, id);
    full.captureCounts[id - 1] = 1;
    Capture& c = full.firstCapture[id - 1]; c.species = id;
    c.at.bootGeneration = 1; c.at.totalSteps = uint64_t(id) * 500;
  }
  for (size_t i = 0; i < 3; ++i) {
    full.lifetime[i].reached = true; full.lifetime[i].first.totalSteps = 251 * kEncounterSteps;
    full.lifetime[i].first.bootGeneration = 1;
  }
  full.recentSize = 32;
  for (size_t i = 0; i < 32; ++i) full.recent[i] = full.firstCapture[219 + i];
  CHECK(validate(full)); const auto fullBytes = encode(full);
  CHECK(fullBytes.size() == 21520 && fullBytes.size() == wideBytes.size() &&
        fullBytes.size() <= codec::maxEncodedSize);
  CHECK(codec::decode(fullBytes.data(), fullBytes.size(), reloaded) == codec::DecodeStatus::Ok);
  CHECK(encode(reloaded) == fullBytes && reloaded.captureCounts[250] == 1 && reloaded.firstCapture[250].species == 251);
  CHECK(!catalogEntry(0) && !catalogEntry(252) && catalogEntry(152) && catalogEntry(251));
  unsigned routes[3] = {0,0,0};
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    const CatalogEntry* e = catalogEntry(id); CHECK(e && e->weight > 0 && e->routes > 0 && e->routes <= 7);
    for (unsigned r = 0; r < 3; ++r) if (e->routes & (1u << r)) ++routes[r];
  }
  CHECK(routes[0] && routes[1] && routes[2]);
  CHECK(captureProbability(false, Timing::Normal) == 55 && captureProbability(false, Timing::Nice) == 70 && captureProbability(false, Timing::Perfect) == 85);
  CHECK(captureProbability(true, Timing::Normal) == 30 && captureProbability(true, Timing::Nice) == 45 && captureProbability(true, Timing::Perfect) == 60);
  CHECK(captureProbability(true, Timing::Miss) == 0);
  std::printf("codec: %u checks passed; v1=%zu, v2=%zu, v3=%zu (max=%zu); pools=%u/%u/%u\n",
              checks, data.size(), twoBytes.size(), fullBytes.size(), codec::maxEncodedSize,
              routes[0], routes[1], routes[2]);
}
