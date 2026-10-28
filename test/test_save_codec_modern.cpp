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
  CHECK(!validate(s)); std::vector<uint8_t> out(codec::maxEncodedSize); size_t n = 123;
  CHECK(!codec::encode(s, out.data(), out.size(), n) && n == 0);
}
struct FixedStore : Store {
  Save saved; bool committed = false;
  LoadResult load(Save& out) override { out = saved; return LoadResult(LoadStatus::Loaded); }
  CommitStatus commit(const Save&) override { committed = true; return CommitStatus::Committed; }
};
// Startup must accept the unmodified baseline and refuse only the broken invariant.
static void acceptAtStartup(const Save& good) {
  FixedStore store; store.saved = good; Engine engine(store);
  CHECK(engine.begin() == CommandResult::Applied && engine.ready() && store.committed);
}
static void rejectAtStartup(const Save& bad) {
  reject(bad);
  FixedStore store; store.saved = bad; Engine engine(store);
  CHECK(engine.begin() == CommandResult::StorageError && !engine.ready() && !store.committed);
}
// Locate the payload byte that distinguishes two encodes of otherwise equal saves.
static size_t fieldOffset(const Save& a, const Save& b) {
  const std::vector<uint8_t> x = encode(a), y = encode(b);
  CHECK(x.size() == y.size());
  size_t offset = 0, diffs = 0;
  for (size_t i = 22; i < x.size(); ++i) if (x[i] != y[i]) { offset = i; ++diffs; }
  CHECK(diffs == 1);
  return offset;
}
// Paint one payload byte of a valid encode and refresh the CRC, so only semantic
// validation (not framing) can reject the result.
static std::vector<uint8_t> forgedAt(const std::vector<uint8_t>& legal, size_t offset, uint8_t value) {
  CHECK(offset >= 22 && offset < legal.size());
  std::vector<uint8_t> bytes = legal; bytes[offset] = value;
  checksum(bytes); return bytes;
}
// Forge the on-disk image of an invalid save: start from a valid encode and paint
// one field byte, so only semantic validation (not framing) can reject it.
static std::vector<uint8_t> forged(const Save& base, const Save& probe, uint8_t value) {
  return forgedAt(encode(base), fieldOffset(base, probe), value);
}
static void rejectBytes(const std::vector<uint8_t>& bytes) {
  Save out; CHECK(codec::decode(bytes.data(), bytes.size(), out) == codec::DecodeStatus::Corrupt);
}
// A rule-2 encounter in Forest with one failed first throw already paid for.
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
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = 5000; s.explorationCredit = 0; s.nextEncounterId = 2;
  s.encounter.id = 1; s.encounter.species = species; s.encounter.partner = 1;
  s.encounter.route = Route::Forest; s.encounter.ruleVersion = 2;
  for (auto& a : s.encounter.attempts) a.ruleVersion = 2;
  s.phase = Phase::EncounterReady;
  return s;
}
// Schema 2 and 3 both own modern encounter state; schema 1 keeps its legacy exemption.
static void modernEncounterBoundaries(const Save& legacy1) {
  for (uint16_t schema : {2, 3}) {
    // #251 is catalogued in Forest but is a retained, non-ordinary species.
    Save base = modernEncounter(schema, 1); CHECK(validate(base)); acceptAtStartup(base);
    CHECK((catalogEntry(251)->routes & 1u) && !ordinaryEncounter(251));
    Save probe = base; probe.encounter.species = 4; CHECK(validate(probe));
    Save retained = base; retained.encounter.species = 251;
    rejectAtStartup(retained);
    rejectBytes(forged(base, probe, 251));
    Save thrown = base; failedFirstThrow(thrown); thrown.ballsSpent = 1;
    CHECK(validate(thrown)); acceptAtStartup(thrown);
    Save paid = thrown; paid.ballsSpent = 2; CHECK(validate(paid));
    Save unpaid = thrown; unpaid.ballsSpent = 0;
    rejectAtStartup(unpaid);
    rejectBytes(forged(thrown, paid, 0));
    Save resolved = base; resolved.phase = Phase::Resolved; resolved.result = Result::Released;
    CHECK(validate(resolved));
    Save lost = resolved; lost.result = Result::Lost; CHECK(validate(lost));
    Save retired = resolved; retired.result = Result::MigrationRetired;
    rejectAtStartup(retired);
    rejectBytes(forged(resolved, lost, uint8_t(Result::MigrationRetired)));
    // A voided legacy boundary must be retired: the books stay consistent, so only
    // the boundary rule itself can reject this state.
    Save boundary = resolved; boundary.voidedLegacyCount = 1; boundary.explorationCredit = 5000;
    boundary.encounter.ruleVersion = 1;
    for (auto& a : boundary.encounter.attempts) a.ruleVersion = 1;
    rejectAtStartup(boundary);
    // Its retired counterpart is legal, so painting only the result byte back to
    // Released must be rejected by decode's semantic check, not by the CRC.
    Save retiredBoundary = boundary;
    retiredBoundary.encounter.species = 251; retiredBoundary.result = Result::MigrationRetired;
    CHECK(validate(retiredBoundary));
    const std::vector<uint8_t> retiredBytes = encode(retiredBoundary);
    Save retiredBack;
    CHECK(codec::decode(retiredBytes.data(), retiredBytes.size(), retiredBack) == codec::DecodeStatus::Ok);
    rejectBytes(forgedAt(retiredBytes, fieldOffset(resolved, lost), uint8_t(Result::Released)));
  }
  // Schema 4: non-legacy encounters run current rules, unlimited balls mean a
  // used throw no longer has to be paid for, and an unmigrated rule-2
  // encounter is refused.
  {
    Save base = modernEncounter(4, 1);
    base.explorationSpent = 5000; base.encounterTarget = 300; base.encounter.ruleVersion = 3;
    CHECK(validate(base)); acceptAtStartup(base);
    Save stale = base; stale.encounter.ruleVersion = 2; rejectAtStartup(stale);
    Save thrown = base; failedFirstThrow(thrown);
    CHECK(thrown.ballsSpent == 0 && thrown.encounter.ruleVersion == 3);
    CHECK(validate(thrown)); acceptAtStartup(thrown);
  }
  // Schema 1 stores no ballsSpent and never owned modern encounters: a legacy
  // encounter with a non-ordinary species and a spent ball must still load.
  Save legacyThrow = legacy1;
  legacyThrow.encounter.id = legacyThrow.nextEncounterId - 1;
  legacyThrow.encounter.species = 251; legacyThrow.encounter.partner = legacyThrow.partner;
  legacyThrow.encounter.route = Route::Forest; legacyThrow.encounter.ruleVersion = 1;
  failedFirstThrow(legacyThrow); CHECK(legacyThrow.ballsSpent == 0);
  CHECK(legacyThrow.schema == 1 && validate(legacyThrow));
  const auto legacyBytes = encode(legacyThrow);
  Save legacyBack; CHECK(codec::decode(legacyBytes.data(), legacyBytes.size(), legacyBack) == codec::DecodeStatus::Ok);
  CHECK(legacyBack.encounter.usedBalls == 1 && legacyBack.ballsSpent == 0);
}
// Schema 1-4 validate routes against the frozen masks; schema 5 uses the typed
// masks and validation never repairs an illegal route on its own.
static void routeMasks() {
  for (uint16_t schema : {2, 3}) {
    Save base = modernEncounter(schema, 4); CHECK(validate(base));
    Save moved = base; moved.encounter.route = Route::Cave; reject(moved);
  }
  Save modern = modernEncounter(4, 4);
  modern.explorationSpent = 5000; modern.encounterTarget = 300; modern.encounter.ruleVersion = 3;
  CHECK(validate(modern));
  Save oldIllegal = modern; oldIllegal.encounter.route = Route::Cave; reject(oldIllegal);
  // #4 小火龙 is Cave-only once types define eligibility.
  Save typed = modernEncounter(5, 4);
  typed.explorationSpent = 5000; typed.encounterTarget = 300; typed.encounter.ruleVersion = 3;
  typed.encounter.route = Route::Cave; CHECK(validate(typed)); acceptAtStartup(typed);
  Save newIllegal = typed; newIllegal.encounter.route = Route::Forest; rejectAtStartup(newIllegal);
  // #92 鬼斯通 is Forest or Cave; painting its stored route to Coast must be
  // rejected by decode instead of being repaired to a legal route.
  Save both = modernEncounter(5, 92);
  both.explorationSpent = 5000; both.encounterTarget = 300; both.encounter.ruleVersion = 3;
  both.encounter.route = Route::Forest; CHECK(validate(both));
  Save stage = both; stage.encounter.route = Route::Cave; CHECK(validate(stage));
  rejectBytes(forged(both, stage, uint8_t(Route::Coast)));
}
int main() {
  // Minimal schema-1 save with a charged legacy encounter boundary.
  Save legacy1; legacy1.schema = 1; legacy1.bootGeneration = 1;
  legacy1.starter = legacy1.partner = 1; setOwned(legacy1, 1);
  legacy1.totalSteps = legacy1.undatedSteps = 5000;
  legacy1.nextEncounterId = 2; legacy1.explorationCredit = 4500;
  legacy1.encounter.ruleVersion = 1;
  for (auto& a : legacy1.encounter.attempts) a.ruleVersion = 1;
  modernEncounterBoundaries(legacy1);
  routeMasks();
  std::printf("save codec modern: %u checks passed\n", checks);
}
