#include "../firmware/apps/20_pokedex/adventure.hpp"
#include "../firmware/apps/20_pokedex/save_codec.hpp"
#include "../firmware/apps/20_pokedex/encounter_catalog.hpp"
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
static void reject(const Save& s) {
  std::vector<uint8_t> out(codec::maxEncodedSize); size_t n = 123;
  CHECK(!codec::encode(s, out.data(), out.size(), n) && n == 0);
}
static size_t fieldOffset(const Save& a, const Save& b) {
  const std::vector<uint8_t> x = encode(a), y = encode(b);
  CHECK(x.size() == y.size());
  size_t offset = 0, diffs = 0;
  for (size_t i = 22; i < x.size(); ++i) if (x[i] != y[i]) { offset = i; ++diffs; }
  CHECK(diffs == 1);
  return offset;
}
static std::vector<uint8_t> forgedAt(const std::vector<uint8_t>& legal, size_t offset, uint8_t value) {
  CHECK(offset >= 22 && offset < legal.size());
  std::vector<uint8_t> bytes = legal; bytes[offset] = value;
  checksum(bytes); return bytes;
}
static std::vector<uint8_t> forged(const Save& base, const Save& probe, uint8_t value) {
  return forgedAt(encode(base), fieldOffset(base, probe), value);
}
static void rejectBytes(const std::vector<uint8_t>& bytes) {
  Save out; out.sequence = 99; out.encounterTarget = 450;
  out.route = Route::Unova; out.stepsIncomplete = true;
  const auto before = encode(out);
  CHECK(codec::decode(bytes.data(), bytes.size(), out) == codec::DecodeStatus::Corrupt);
  CHECK(encode(out) == before);
}
static void failedFirstThrow(Save& s) {
  const uint16_t rule = s.encounter.ruleVersion;
  const uint8_t probability = captureProbability(rule, catalogEntry(s.encounter.species)->rare, Timing::Normal);
  s.phase = Phase::ThrowReady; s.result = Result::Won; s.encounter.usedBalls = 1;
  s.encounter.seed = 0;
  while (throwRoll(s.encounter, 1, rule) < probability) ++s.encounter.seed;
  ThrowAttempt& a = s.encounter.attempts[0];
  a.used = true; a.timing = Timing::Normal; a.probability = probability;
  a.roll = throwRoll(s.encounter, 1, rule); a.captured = a.roll < probability;
  a.ruleVersion = rule;
  CHECK(!a.captured);
}
static Save modernEncounter(uint16_t schema, uint16_t species) {
  Save s; s.schema = schema; s.sequence = 7; s.bootGeneration = 1; s.starter = 1; s.partner = 1;
  s.route = Route::Forest; s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = 5000; s.explorationCredit = 0; s.nextEncounterId = 2;
  s.encounter.id = 1; s.encounter.species = species; s.encounter.partner = 1;
  s.encounter.route = Route::Forest; s.encounter.ruleVersion = 2;
  for (auto& a : s.encounter.attempts) a.ruleVersion = 2;
  s.phase = Phase::EncounterReady;
  return s;
}
static void modernEncounterBoundaries(const Save& legacy1) {
  for (uint16_t schema : {2, 3}) {
    const uint16_t excluded = schema == 2 ? 151 : 251;
    Save base = modernEncounter(schema, 1); CHECK(validate(base));
    CHECK(excluded <= speciesCapacity(schema).count);
    CHECK((savedRouteMask(excluded, schema) & 1u) && !ordinaryEncounter(excluded));
    Save probe = base; probe.encounter.species = 4; CHECK(validate(probe));
    rejectBytes(forged(base, probe, uint8_t(excluded)));
    Save thrown = base; failedFirstThrow(thrown); thrown.ballsSpent = 1;
    CHECK(validate(thrown));
    Save paid = thrown; paid.ballsSpent = 2; CHECK(validate(paid));
    rejectBytes(forged(thrown, paid, 0));
    Save resolved = base; resolved.phase = Phase::Resolved; resolved.result = Result::Released;
    CHECK(validate(resolved));
    Save lost = resolved; lost.result = Result::Lost; CHECK(validate(lost));
    rejectBytes(forged(resolved, lost, uint8_t(Result::MigrationRetired)));
    Save boundary = resolved; boundary.voidedLegacyCount = 1; boundary.explorationCredit = 5000;
    boundary.encounter.ruleVersion = 1;
    for (auto& a : boundary.encounter.attempts) a.ruleVersion = 1;
    Save retiredBoundary = boundary;
    retiredBoundary.encounter.species = schema == 2 ? 151 : 251; retiredBoundary.result = Result::MigrationRetired;
    CHECK(validate(retiredBoundary));
    const std::vector<uint8_t> retiredBytes = encode(retiredBoundary);
    Save retiredBack;
    CHECK(codec::decode(retiredBytes.data(), retiredBytes.size(), retiredBack) == codec::DecodeStatus::Ok);
    rejectBytes(forgedAt(retiredBytes, fieldOffset(resolved, lost), uint8_t(Result::Released)));
  }
  {
    Save base = modernEncounter(4, 1);
    base.explorationSpent = 5000; base.encounterTarget = 300; base.encounter.ruleVersion = 3;
    CHECK(validate(base));
    Save stale = base; stale.encounter.ruleVersion = 2; reject(stale);
    Save thrown = base; failedFirstThrow(thrown);
    CHECK(thrown.ballsSpent == 0 && thrown.encounter.ruleVersion == 3);
    CHECK(validate(thrown));
  }
  Save legacyThrow = legacy1;
  legacyThrow.encounter.id = legacyThrow.nextEncounterId - 1;
  legacyThrow.encounter.species = 151; legacyThrow.encounter.partner = legacyThrow.partner;
  legacyThrow.encounter.route = Route::Forest; legacyThrow.encounter.ruleVersion = 1;
  failedFirstThrow(legacyThrow); CHECK(legacyThrow.ballsSpent == 0);
  CHECK(legacyThrow.schema == 1 && validate(legacyThrow));
  const auto legacyBytes = encode(legacyThrow);
  Save legacyBack; CHECK(codec::decode(legacyBytes.data(), legacyBytes.size(), legacyBack) == codec::DecodeStatus::Ok);
  CHECK(legacyBack.encounter.usedBalls == 1 && legacyBack.ballsSpent == 0);
}
static void routeMasks() {
  for (uint16_t schema : {2, 3}) {
    Save base = modernEncounter(schema, 4); CHECK(validate(base));
    Save moved = base; moved.encounter.route = Route::Cave; reject(moved);
  }
  Save modern = modernEncounter(4, 4);
  modern.explorationSpent = 5000; modern.encounterTarget = 300; modern.encounter.ruleVersion = 3;
  CHECK(validate(modern));
  Save oldIllegal = modern; oldIllegal.encounter.route = Route::Cave; reject(oldIllegal);
  Save typed = modernEncounter(5, 4);
  typed.explorationSpent = 5000; typed.encounterTarget = 300; typed.encounter.ruleVersion = 3;
  typed.encounter.route = Route::Cave; CHECK(validate(typed));
  Save newIllegal = typed; newIllegal.encounter.route = Route::Forest; reject(newIllegal);
  Save both = modernEncounter(5, 92);
  both.explorationSpent = 5000; both.encounterTarget = 300; both.encounter.ruleVersion = 3;
  both.encounter.route = Route::Forest; CHECK(validate(both));
  Save stage = both; stage.encounter.route = Route::Cave; CHECK(validate(stage));
  rejectBytes(forged(both, stage, uint8_t(Route::Coast)));
  Save regional = modernEncounter(7, 4);
  regional.route = Route::Hoenn; regional.encounter.route = Route::Kanto;
  regional.explorationSpent = 5000; regional.encounterTarget = 300; regional.encounter.ruleVersion = 3;
  CHECK(validate(regional));
  Save johto = modernEncounter(7, 152);
  johto.encounter.route = Route::Johto; johto.explorationSpent = 5000; johto.encounterTarget = 300;
  johto.encounter.ruleVersion = 3; johto.route = Route::Kanto; CHECK(validate(johto));
  for (Route wrong : {Route::Forest, Route::Cave, Route::Johto, Route::Unova}) {
    Save bad = regional; bad.encounter.route = wrong; reject(bad);
  }
  Save selection = regional; selection.route = Route::Cave; reject(selection);
  Save probe = regional; probe.route = Route::Sinnoh; CHECK(validate(probe));
  rejectBytes(forged(regional, probe, uint8_t(Route::Coast)));
  rejectBytes(forged(regional, probe, 8));
  Save history = regional;
  history.encounter = Encounter(); history.phase = Phase::Walking;
  history.captureCounts[3] = 1;
  Capture caught; caught.species = 4; caught.route = Route::Cave; caught.at.bootGeneration = 1; caught.at.totalSteps = 500;
  history.firstCapture[3] = caught; history.recent[0] = caught; history.recentSize = 1;
  setRegistered(history, 4); CHECK(validate(history));
  Save beyond = history; beyond.firstCapture[3].route = Route(8); reject(beyond);
}
int main() {
  Save legacy1; legacy1.schema = 1; legacy1.route = Route::Forest; legacy1.bootGeneration = 1;
  legacy1.starter = legacy1.partner = 1; setOwned(legacy1, 1);
  legacy1.totalSteps = legacy1.undatedSteps = 5000;
  legacy1.nextEncounterId = 2; legacy1.explorationCredit = 4500;
  legacy1.encounter.ruleVersion = 1;
  for (auto& a : legacy1.encounter.attempts) a.ruleVersion = 1;
  modernEncounterBoundaries(legacy1);
  routeMasks();
  std::printf("save codec modern: %u checks passed\n", checks);
}
