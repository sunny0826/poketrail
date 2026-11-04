#include "support/progression/harness.hpp"
#include "adventure_migration.hpp"
#include <algorithm>

static void checksum(std::vector<uint8_t>& bytes) {
  std::vector<uint8_t> covered(bytes.begin(), bytes.begin() + 18);
  covered.insert(covered.end(), bytes.begin() + 22, bytes.end());
  const uint32_t crc = codec::crc32(covered.data(), covered.size());
  for (unsigned i = 0; i < 4; ++i) bytes[18 + i] = uint8_t(crc >> (8 * i));
}
static void sameEncounter(const Encounter& a, const Encounter& b) {
  CHECK(a.id == b.id && a.species == b.species && a.partner == b.partner &&
        a.route == b.route && a.seed == b.seed && a.ruleVersion == b.ruleVersion);
}
// Banked exploration fills only available slots; every snapshot is published
// atomically, remains unique, and is served FIFO even after roster changes.
static void capacityAndFifo() {
  auto save = fixture(1, 20000); save->progress[0].walked = 20000;
  setOwned(*save, 4); setRegistered(*save, 4);
  MemoryStore store; store.disk = encoded(*save); Engine e(store);
  CHECK(e.begin() == CommandResult::Applied);
  CHECK(steps(e, 1, 1) == CommandResult::Applied);
  CHECK(pendingEncounterCount(e.view()) == 32 && e.view().encounterQueueSize == 31);
  const uint64_t spent = e.view().explorationSpent, credit = e.view().explorationCredit;
  CHECK(credit > e.view().encounterTarget && credit + spent == 20001);
  CHECK(steps(e, 10, 1) == CommandResult::Applied);
  CHECK(e.view().explorationSpent == spent && e.view().explorationCredit == credit + 10);
  const Save frozen = e.view();
  CHECK(e.selectPartner(4) == CommandResult::Applied);
  sameEncounter(e.view().encounter, frozen.encounter);
  for (size_t i = 0; i < 31; ++i) sameEncounter(e.view().encounterQueue[i], frozen.encounterQueue[i]);
  CHECK(e.selectPartner(1) == CommandResult::Applied);
  CHECK(e.evolve(request(e, 1, 2), clockFor(e)) == CommandResult::Applied);
  sameEncounter(e.view().encounter, frozen.encounter);
  for (size_t i = 0; i < 31; ++i) sameEncounter(e.view().encounterQueue[i], frozen.encounterQueue[i]);
  CHECK(!owns(e.view(), 1) && registered(e.view(), 1));
  CHECK(e.startBattle(0) == CommandResult::Applied && pendingEncounterCount(e.view()) == 31);
  CHECK(e.retreat() == CommandResult::Applied && pendingEncounterCount(e.view()) == 31);
  CHECK(e.acknowledgeResult(frozen.encounter.id, 456) == CommandResult::Applied);
  sameEncounter(e.view().encounter, frozen.encounterQueue[0]);
  CHECK(pendingEncounterCount(e.view()) == 32);
  CHECK(e.view().encounterQueue[30].partner == 2);
  CHECK(e.view().explorationSpent == spent + frozen.encounterTarget);
  CHECK(e.checkpoint() == CommandResult::Duplicate);
  Engine reboot(store); CHECK(reboot.begin() == CommandResult::Applied);
  sameEncounter(reboot.view().encounter, frozen.encounterQueue[0]);
  CHECK(reboot.startBattle(0) == CommandResult::Applied);
  CHECK(reboot.checkpoint() == CommandResult::Deferred);
  CHECK(reboot.retreat() == CommandResult::Applied);
}
// A failed append or FIFO pop must retry byte-for-byte, without revealing it.
static void retryAndRecovery() {
  MemoryStore store; Engine e(store); CHECK(e.begin() == CommandResult::Applied);
  CHECK(e.chooseStarter(1) == CommandResult::Applied);
  CHECK(steps(e, e.view().encounterTarget, 1) == CommandResult::Applied);
  CHECK(steps(e, e.view().encounterTarget - 1, 1) == CommandResult::Applied);
  const auto visible = encoded(e.view()); store.before = true;
  CHECK(steps(e, 1, 1) == CommandResult::StorageError);
  const auto frozen = store.attempt;
  CHECK(encoded(e.view()) == visible && pendingEncounterCount(e.view()) == 1);
  CHECK(e.retrySave() == CommandResult::StorageError && store.attempt == frozen);
  store.before = false; CHECK(e.retrySave() == CommandResult::Applied && store.disk == frozen);
  CHECK(pendingEncounterCount(e.view()) == 2);
  CHECK(e.retreat() == CommandResult::Applied);
  const auto resolved = encoded(e.view()); const Encounter next = e.view().encounterQueue[0];
  store.before = true;
  CHECK(e.acknowledgeResult(e.view().encounter.id, 999) == CommandResult::StorageError);
  CHECK(encoded(e.view()) == resolved); const auto pop = store.attempt;
  CHECK(e.retrySave() == CommandResult::StorageError && store.attempt == pop);
  store.before = false; CHECK(e.retrySave() == CommandResult::Applied && store.disk == pop);
  sameEncounter(e.view().encounter, next);
  CHECK(e.startBattle(0) == CommandResult::Applied);
  // Paused battles can checkpoint and reboot back into EncounterReady.
  CHECK(e.requestPause() == CommandResult::Applied);
  CHECK(e.tick(e.battleView().targetMs + 181, clockFor(e)) == CommandResult::Applied);
  CHECK(e.checkpoint() == CommandResult::Applied);
  Engine reboot(store); CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.view().phase == Phase::EncounterReady); sameEncounter(reboot.view().encounter, next);
}
// Correct CRC cannot authorize duplicate species, reordered ids, stale attempt
// data, wrong region/partner, oversized counts, or a hidden old-schema queue.
static void semanticCorruption() {
  auto save = fixture(1, 3000); MemoryStore store; store.disk = encoded(*save); Engine e(store);
  CHECK(e.begin() == CommandResult::Applied && steps(e, 1, 1) == CommandResult::Applied);
  const auto legal = encoded(e.view());
  constexpr size_t width = 35;
  const size_t start = legal.size() - 1 - 31 * width;
  for (unsigned kind = 0; kind < 8; ++kind) {
    auto bad = legal;
    if (kind == 0) bad[start] = 32;
    if (kind == 1) bad[start + 1] = 0;
    if (kind == 2) { bad[start + 9] = uint8_t(e.view().encounter.species); bad[start + 10] = 0; }
    if (kind == 3) bad[start + 11] = 0;
    if (kind == 4) bad[start + 13] = uint8_t(Route::Unova);
    if (kind == 5) bad[start + 20] = 1;
    if (kind == 6) bad[start + 22] = 1;
    if (kind == 7) bad.back() = 1;
    checksum(bad); Save out;
    CHECK(codec::decode(bad.data(), bad.size(), out) == codec::DecodeStatus::Corrupt);
  }
  Save old = e.view(); old.schema = 8; CHECK(!validate(old));
  size_t written = 99; std::vector<uint8_t> bytes(codec::maxEncodedSize);
  CHECK(!codec::encode(old, bytes.data(), bytes.size(), written) && written == 0);
}
// Schema8 retains the old payload and encounter/ball state on migration.
static void schema8Migration() {
  auto old = fixture(1, 1000); old->schema = 8; old->nextEncounterId = 2;
  old->legacyChargedCount = 1; old->explorationSpent = 500; old->explorationCredit = 500;
  old->encounter.id = 1; old->encounter.species = 4; old->encounter.partner = 1;
  old->encounter.route = Route::Kanto; old->encounter.seed = 123; old->encounter.ruleVersion = 1;
  for (auto& a : old->encounter.attempts) a.ruleVersion = 1;
  old->phase = Phase::ThrowReady; old->result = Result::Won; old->ballsSpent = 1;
  // No attempt was charged in this scene; ballsSpent is historical inventory.
  // Frozen bytes emitted by the unmodified c5a99dd schema8 codec. This
  // independent historical frame guards against matching encoder/decoder drift.
  struct Byte { size_t offset; uint8_t value; };
  const Byte nonzero[] = {
    {0,65},{1,86},{2,68},{3,49},{4,8},{6,19},{14,11},{15,190},
    {18,118},{19,148},{20,35},{21,169},{22,1},{24,1},{26,3},{27,1},
    {33857,232},{33858,3},{33865,244},{33866,1},{33873,232},{33874,3},
    {38042,1},{38054,2},{38062,3},{38063,1},{38064,1},{38072,4},{38074,1},
    {38076,3},{38077,123},{38081,1},{38089,1},{38096,1},{38098,1},
    {48572,1},{48580,1},{48589,1},{48662,44},{48663,1},{48664,244},{48665,1}
  };
  std::vector<uint8_t> bytes(48673);
  for (const auto b : nonzero) bytes[b.offset] = b.value;
  old->sequence = 19;
  CHECK(encoded(*old) == bytes);
  Save decoded;
  CHECK(codec::decode(bytes.data(), bytes.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(decoded.schema == 8 && encoded(decoded) == bytes);
  CHECK(migrateLegacy(decoded) && decoded.schema == 9 && !decoded.encounterQueueSize);
  sameEncounter(decoded.encounter, old->encounter);
  CHECK(decoded.phase == old->phase && decoded.ballsSpent == old->ballsSpent &&
        decoded.explorationCredit == old->explorationCredit && decoded.nextEncounterId == old->nextEncounterId);
  Save invalid = *old; invalid.encounter.partner = 2; setRegistered(invalid, 2);
  CHECK(!validate(invalid)); invalid.schema = 9; CHECK(validate(invalid));
}
static void exhaustedPool() {
  auto save = fixture(1, 3000);
  for (uint16_t id = 1; id <= kSpeciesCount; ++id)
    if (id < 4 || id > 9) setRegistered(*save, id);
  MemoryStore store; store.disk = encoded(*save); Engine e(store);
  CHECK(e.begin() == CommandResult::Applied && steps(e, 1, 1) == CommandResult::Applied);
  CHECK(pendingEncounterCount(e.view()) == 2 && remainingEncounters(e.view(), Route::Kanto) == 0);
  CHECK(e.view().encounter.species != e.view().encounterQueue[0].species);
  const uint64_t spent = e.view().explorationSpent, credit = e.view().explorationCredit;
  CHECK(credit >= e.view().encounterTarget);
  CHECK(steps(e, 5, 1) == CommandResult::Applied && e.view().explorationSpent == spent &&
        e.view().explorationCredit == credit + 5);
  const auto current = e.view().encounter; const auto queued = e.view().encounterQueue[0];
  CHECK(e.selectRoute(Route::Johto) == CommandResult::Applied);
  sameEncounter(e.view().encounter, current); sameEncounter(e.view().encounterQueue[0], queued);
}
static void throwBufferedThreshold() {
  MemoryStore store; Engine e(store); CHECK(e.begin() == CommandResult::Applied);
  CHECK(e.chooseStarter(1) == CommandResult::Applied);
  CHECK(steps(e, e.view().encounterTarget, 1) == CommandResult::Applied);
  win(e);
  CHECK(steps(e, e.view().encounterTarget - 1, 1) == CommandResult::Applied);
  const auto current = e.view().encounter;
  CHECK(e.beginThrow(AttemptKey(current.id, 1), 0) == CommandResult::Applied);
  StepEvent step; step.partner = 1; step.batchId = e.view().lastStepBatch + 1; step.clock = clockFor(e);
  CHECK(e.observeSteps(&step, 1, 777) == CommandResult::Deferred);
  store.before = true;
  CHECK(e.tick(e.throwView().targetMs + 181, clockFor(e)) == CommandResult::StorageError);
  CHECK(!e.view().encounterQueueSize && e.view().encounter.usedBalls == 0);
  const auto frozen = store.attempt;
  CHECK(e.retrySave() == CommandResult::StorageError && store.attempt == frozen);
  store.before = false; CHECK(e.retrySave() == CommandResult::Applied);
  CHECK(e.view().encounterQueueSize == 1 && e.view().encounter.usedBalls == 1);
  sameEncounter(e.view().encounter, current);
  CHECK(e.view().encounterQueue[0].seed == 777 && pendingEncounterCount(e.view()) == 1);
}
int main() {
  capacityAndFifo(); retryAndRecovery(); semanticCorruption(); schema8Migration(); exhaustedPool();
  throwBufferedThreshold();
  std::printf("encounter queue: %u checks passed\n", checks);
}
