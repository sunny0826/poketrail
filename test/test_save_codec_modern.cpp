#include "../firmware/apps/20_pokedex/adventure.hpp"
#include "../firmware/apps/20_pokedex/save_codec.hpp"
#include "../firmware/apps/20_pokedex/encounter_catalog.hpp"
#include "../firmware/apps/20_pokedex/ruins_catalog.hpp"
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
  const uint8_t probability = captureProbability(catalogEntry(s.encounter.species)->rare, Timing::Normal);
  s.phase = Phase::ThrowReady; s.result = Result::Won; s.encounter.usedBalls = 1;
  s.encounter.seed = 0;
  while (throwRoll(s.encounter, 1) < probability) ++s.encounter.seed;
  ThrowAttempt& a = s.encounter.attempts[0];
  a.used = true; a.timing = Timing::Normal; a.probability = probability;
  a.roll = throwRoll(s.encounter, 1); a.captured = a.roll < probability;
  a.ruleVersion = s.encounter.ruleVersion;
  CHECK(!a.captured);
}
static Save modernEncounter(uint16_t schema, uint16_t species) {
  Save s; s.schema = schema; s.sequence = 7; s.bootGeneration = 1; s.starter = 1; s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = 5000; s.explorationCredit = 0; s.nextEncounterId = 2;
  s.encounter.id = 1; s.encounter.species = species; s.encounter.partner = 1;
  s.encounter.route = Route::Forest; s.encounter.ruleVersion = 2; s.phase = Phase::EncounterReady;
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
  // A schema-3 ruins boss encounter is legal only with its catalog target and a
  // solved unlock, and it does not consume the ordinary 5000-step credit.
  Save activity = modernEncounter(3, 251);
  activity.activityEncounterCount = 1; activity.explorationCredit = 5000;
  activity.encounter.opponent = 251; activity.encounter.origin = EncounterOrigin::Ruins;
  activity.ruins.clueMask = kRuinsAllClues; activity.ruins.unlocked = true;
  activity.ruins.lastTarget = 251;
  CHECK(validate(activity)); acceptAtStartup(activity);
  Save activitySchema2 = activity; activitySchema2.schema = 2;
  rejectAtStartup(activitySchema2);
  // An activity-issued ID still booked as an ordinary encounter: the same books
  // are legal with an activity origin, so only the ordinary-ID rule can reject it.
  // The current field is a fixed Forest clue battle (opponent 10).
  Save activityBook = modernEncounter(3, 10);
  activityBook.activityEncounterCount = 1; activityBook.explorationCredit = 5000;
  activityBook.encounter.opponent = 10; activityBook.encounter.origin = EncounterOrigin::Ruins;
  CHECK(validate(activityBook)); acceptAtStartup(activityBook);
  // The frozen schema-3 activity block puts origin right after the 8-byte count.
  const auto originOffset = [](const std::vector<uint8_t>& bytes) { return bytes.size() - 22 + 8; };
  Save dragonBook = activityBook; dragonBook.encounter.origin = EncounterOrigin::DragonTrial;
  reject(dragonBook);
  const auto bookBytes = encode(activityBook);
  Save bookBack;
  CHECK(codec::decode(bookBytes.data(), bookBytes.size(), bookBack) == codec::DecodeStatus::Ok);
  rejectBytes(forgedAt(bookBytes, originOffset(bookBytes), uint8_t(EncounterOrigin::Ordinary)));
  Save ordinaryBook = activityBook; ordinaryBook.encounter.origin = EncounterOrigin::Ordinary;
  rejectAtStartup(ordinaryBook);
  // A legacy-charged boundary must not excuse the current ordinary field.
  Save legacyMix = activityBook;
  legacyMix.totalSteps = legacyMix.undatedSteps = 5500;
  legacyMix.nextEncounterId = 3; legacyMix.encounter.id = 2; legacyMix.legacyChargedCount = 1;
  CHECK(validate(legacyMix)); acceptAtStartup(legacyMix);
  Save legacyDragon = legacyMix; legacyDragon.encounter.origin = EncounterOrigin::DragonTrial;
  reject(legacyDragon);
  const auto legacyBookBytes = encode(legacyMix);
  rejectBytes(forgedAt(legacyBookBytes, originOffset(legacyBookBytes),
                       uint8_t(EncounterOrigin::Ordinary)));
  Save legacyUnpaid = legacyMix; legacyUnpaid.encounter.origin = EncounterOrigin::Ordinary;
  rejectAtStartup(legacyUnpaid);
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
// A legal Forest clue battle, resolved by the public Engine commands. The two
// games below are the only sources of the clue snapshots used by the invariant
// tests, so a forged field is a single byte away from a real state.
static Save ruinsClueBattleSave() {
  Save s; s.schema = 3; s.sequence = 12; s.bootGeneration = 1; s.starter = 1; s.partner = 1;
  s.rosterRevision = 1; setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.undatedSteps = 5000; s.explorationCredit = 5000;
  s.nextEncounterId = 2; s.activityEncounterCount = 1;
  s.encounter.id = 1; s.encounter.species = s.encounter.opponent = 10;
  s.encounter.partner = 1; s.encounter.route = Route::Forest; s.encounter.ruleVersion = 2;
  s.encounter.origin = EncounterOrigin::Ruins; s.phase = Phase::EncounterReady;
  CHECK(validate(s));
  return s;
}
static Save ruinsClueSnapshot(bool win) {
  FixedStore store; store.saved = ruinsClueBattleSave();
  Engine engine(store); CHECK(engine.begin() == CommandResult::Applied);
  CHECK(engine.startBattle(1000) == CommandResult::Applied);
  for (unsigned guard = 0; engine.view().phase == Phase::Battle && guard < 16; ++guard) {
    const BattleView view = engine.battleView();
    if (win) CHECK(engine.press(view.expected, view.targetMs) == CommandResult::Applied);
    if (engine.view().phase != Phase::Battle) break;
    ClockReading clock; clock.bootGeneration = engine.view().bootGeneration;
    CHECK(engine.tick(view.targetMs + kNormalWindowMs + 1, clock) == CommandResult::Applied);
  }
  CHECK(engine.view().phase == Phase::Resolved);
  return engine.view();
}
// The current clue's bit and the terminal result must agree: only RuinsClue may
// carry the bit; an unresolved, Lost or Retreated run may not. Other routes'
// bits stay legal because they are independent evidence already earned.
static void ruinsClueResultInvariant() {
  const Save win = ruinsClueSnapshot(true);
  CHECK(win.result == Result::RuinsClue && (win.ruins.clueMask & 1) && !win.ruins.unlocked);
  CHECK(validate(win)); acceptAtStartup(win);
  Save winAgain; const auto winBytes = encode(win);
  CHECK(codec::decode(winBytes.data(), winBytes.size(), winAgain) == codec::DecodeStatus::Ok);

  const Save lost = ruinsClueSnapshot(false);
  CHECK(lost.result == Result::Lost && !(lost.ruins.clueMask & 1));
  CHECK(validate(lost)); acceptAtStartup(lost);
  const auto lostBytes = encode(lost);

  Save retreated = lost; retreated.result = Result::Retreated; CHECK(validate(retreated));
  const size_t resultByte = fieldOffset(lost, retreated);
  Save otherBit = lost; otherBit.ruins.clueMask = 2; CHECK(validate(otherBit));
  const size_t maskByte = fieldOffset(lost, otherBit);

  // Real success painted back to a failure result: rejected by validate, by the
  // encoder, by decode after a fresh CRC, and by startup.
  for (Result result : {Result::Lost, Result::Retreated}) {
    Save bad = win; bad.result = result;
    reject(bad); rejectAtStartup(bad);
    rejectBytes(forgedAt(winBytes, resultByte, uint8_t(result)));
  }
  // Real failure snapshots that suddenly own this run's clue bit.
  const Save* failures[] = {&lost, &retreated};
  for (const Save* base : failures) {
    Save bad = *base; bad.ruins.clueMask |= 1;
    reject(bad); rejectAtStartup(bad);
    const auto bytes = encode(*base);
    rejectBytes(forgedAt(bytes, maskByte, uint8_t(base->ruins.clueMask | 1)));
  }
  // An unresolved run has not banked this clue either.
  const Save pending = ruinsClueBattleSave();
  Save pendingBattleStart = pending; pendingBattleStart.phase = Phase::Battle;
  const Save pendingBattle = pendingBattleStart; CHECK(validate(pendingBattle));
  for (const Save* base : {&pending, &pendingBattle}) {
    Save bad = *base; bad.ruins.clueMask |= 1;
    reject(bad); rejectAtStartup(bad);
    const auto bytes = encode(*base);
    rejectBytes(forgedAt(bytes, maskByte, 1));
  }
  // Positives keep the neighbouring route bits and the later clue wins legal.
  Save pendingOther = pending; pendingOther.ruins.clueMask = 2;
  CHECK(validate(pendingOther)); acceptAtStartup(pendingOther);
  Save winAll = win; winAll.ruins.clueMask = kRuinsAllClues;
  CHECK(validate(winAll)); acceptAtStartup(winAll);
  Save lostOther = lost; lostOther.ruins.clueMask = 6;
  CHECK(validate(lostOther)); acceptAtStartup(lostOther);
  std::puts("PASS clue bit and result agree for success, failure and unresolved saves");
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
  ruinsClueResultInvariant();
  std::printf("save codec modern: %u checks passed\n", checks);
}
