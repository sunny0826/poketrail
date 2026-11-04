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
static size_t firstPayloadDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  CHECK(a.size() == b.size());
  for (size_t i = 22; i < a.size(); ++i) if (a[i] != b[i]) return i;
  CHECK(false); return 0;
}
static void reject(const Save& s) {
  std::vector<uint8_t> out(codec::maxEncodedSize); size_t n = 123;
  CHECK(!codec::encode(s, out.data(), out.size(), n) && n == 0);
}
// A rejected load must not replace any part of the player's current save.
static void rejectBytes(const uint8_t* data, size_t size,
                        codec::DecodeStatus status = codec::DecodeStatus::Corrupt) {
  static const Save sentinel = [] {
    Save out; out.sequence = 0xfedcba9876543210ULL; out.bootGeneration = 9;
    out.starter = out.partner = 4; out.rosterRevision = 7;
    setRegistered(out, 4); setOwned(out, 4); out.progress[3].walked = 17;
    out.totalSteps = out.undatedSteps = out.explorationCredit = 17;
    out.route = Route::Unova; out.encounterTarget = 450; out.stepsIncomplete = true;
    return out;
  }();
  static const std::vector<uint8_t> before = encode(sentinel);
  Save out = sentinel;
  CHECK(codec::decode(data, size, out) == status);
  CHECK(encode(out) == before);
}
int main() {
  Save s; s.sequence = 0x0102030405060708ULL; s.bootGeneration = 1;
  s.encounterTarget = 300; s.explorationSpent = 0;
  CHECK(validate(s)); const std::vector<uint8_t> original = encode(s);
  Save decoded; CHECK(codec::decode(original.data(), original.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(encode(decoded) == original && decoded.sequence == s.sequence);
  for (size_t n = 0; n < original.size(); ++n) rejectBytes(original.data(), n);
  rejectBytes(nullptr, original.size());
  std::vector<uint8_t> data = original; data.push_back(0);
  rejectBytes(data.data(), data.size());
  data.resize(codec::maxEncodedSize + 1);
  rejectBytes(data.data(), data.size());
  data = original; data[4] = uint8_t(kSchemaVersion + 1);
  rejectBytes(data.data(), data.size(), codec::DecodeStatus::UnsupportedVersion);
  data.resize(codec::maxEncodedSize + 100);
  rejectBytes(data.data(), data.size(), codec::DecodeStatus::UnsupportedVersion);
  for (size_t i = 0; i < original.size(); ++i) {
    if (i == 4 || i == 5) continue;
    data = original; data[i] ^= 0x80;
    rejectBytes(data.data(), data.size());
  }
  for (uint8_t route : {0, 1, 2, 8}) {  // schema 7 stores a region, never a legacy type route
    data = original; data[22 + 4] = route; checksum(data);
    rejectBytes(data.data(), data.size());
  }
  data = original; data[22 + 4] = uint8_t(Route::Unova); checksum(data);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Ok && decoded.route == Route::Unova);
  Save toggled = s; toggled.stepsIncomplete = true; const auto incomplete = encode(toggled);
  data = original; data[firstPayloadDiff(original, incomplete)] = 2; checksum(data); // bool is 0/1
  rejectBytes(data.data(), data.size());
  Save changed = s; changed.clock.pending.mode = ClockChangeMode::RepairWrongDate;
  const auto mode = encode(changed);
  data = original; data[firstPayloadDiff(original, mode)] = 3; checksum(data);
  rejectBytes(data.data(), data.size());
  std::vector<uint8_t> small(original.size() - 1); size_t n = 12;
  CHECK(!codec::encode(s, small.data(), small.size(), n) && n == 0);
  CHECK(!codec::encode(s, nullptr, 0, n));
  Save bad = s; bad.route = Route::Forest; reject(bad);
  bad = s; bad.route = Route(8); reject(bad);
  bad = s; bad.partner = 151; reject(bad);
  bad = s; bad.owned[kSpeciesBytes - 1] = 0x02; reject(bad);
  bad = s; bad.registered[kSpeciesBytes - 1] = 0x02; reject(bad);
  // Owned, walked and captured species must be registered. Start from a baseline
  // where #200 is registered (bit 199 = byte 24, mask 0x80) and clear only that bit.
  Save gated; gated.sequence = 3; gated.bootGeneration = 1; gated.starter = gated.partner = 1;
  gated.rosterRevision = 1; setRegistered(gated, 1); setOwned(gated, 1);
  gated.encounterTarget = 300;
  setRegistered(gated, 200); gated.totalSteps = gated.undatedSteps = gated.explorationCredit = 5000;
  // Registration and partner ownership are valid; only the starter choice is illegal.
  Save starter = gated; setRegistered(starter, 2); CHECK(validate(starter));
  bad = starter; bad.starter = 2; reject(bad);
  Save owned200 = gated; setOwned(owned200, 200); CHECK(validate(owned200));
  bad = owned200; bad.registered[24] &= uint8_t(~0x80); reject(bad);
  Save walked200 = gated; walked200.progress[199].walked = 1; CHECK(validate(walked200));
  bad = walked200; bad.registered[24] &= uint8_t(~0x80); reject(bad);
  Save caught200 = gated; caught200.captureCounts[199] = 1;
  caught200.firstCapture[199].species = 200;
  caught200.firstCapture[199].at.bootGeneration = 1; caught200.firstCapture[199].at.totalSteps = 500;
  caught200.recent[0] = caught200.firstCapture[199]; caught200.recentSize = 1;
  caught200.nextEncounterId = 2; caught200.explorationCredit = 0; caught200.explorationSpent = 5000;
  CHECK(validate(caught200));
  bad = caught200; bad.registered[24] &= uint8_t(~0x80); reject(bad);
  // A real capture requires its first record and cannot come from a future boot.
  bad = caught200; bad.firstCapture[199] = Capture(); reject(bad);
  bad = caught200; bad.firstCapture[199].at.bootGeneration = caught200.bootGeneration + 1; reject(bad);
  bad = s; bad.historySize = 15; reject(bad);
  bad = s; bad.recentSize = 33; reject(bad);
  bad = s; bad.phase = Phase::ThrowReady; reject(bad);
  bad = s; bad.encounter.usedBalls = 3; reject(bad);
  bad = s; bad.clock.pending.active = true; reject(bad);
  bad = s; bad.clock.trusted = true; reject(bad);
  bad = s; bad.today.dayKey = 20260229; reject(bad);
  bad = s; bad.clock.offsetMinutes = 841; reject(bad);
  bad = s; bad.explorationCredit = 1; reject(bad);
  bad = s; bad.nextEncounterId = 2; reject(bad);
  bad = s; bad.clock.anchor.bootGeneration = 2; reject(bad);
  bad = s; bad.clock.pending.transactionId = 1; reject(bad);
  ClockReading extreme; extreme.trusted = true; extreme.dayKey = 20261022; extreme.offsetMinutes = 840;
  extreme.utcSeconds = INT64_MAX; CHECK(!validReading(extreme));
  extreme.utcSeconds = INT64_MIN; extreme.offsetMinutes = -720; CHECK(!validReading(extreme));
  bad = s; bad.clock.pending.active = true; bad.clock.pending.transactionId = 1;
  bad.clock.pending.targetDayKey = 20261022; bad.clock.pending.targetOffsetMinutes = 840;
  bad.clock.pending.targetUtcSeconds = INT64_MAX; reject(bad);
  s.clock.offsetMinutes = -720;
  data = encode(s); CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(decoded.clock.offsetMinutes == -720 && encode(decoded) == data);
  s.schema = 1; s.route = Route::Forest; s.encounter.ruleVersion = 1;
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
  Save full; full.sequence = 1; full.bootGeneration = 1; full.starter = full.partner = 1;
  full.rosterRevision = 1; full.totalSteps = full.undatedSteps = kSpeciesCount * kLegacyEncounterSteps;
  full.explorationSpent = full.totalSteps; full.encounterTarget = 400;
  full.nextEncounterId = kSpeciesCount + 1; full.explorationCredit = 0;
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    setRegistered(full, id); setOwned(full, id);
    full.captureCounts[id - 1] = 1;
    Capture& c = full.firstCapture[id - 1]; c.species = id;
    c.at.bootGeneration = 1; c.at.totalSteps = uint64_t(id) * 500;
  }
  for (size_t i = 0; i < 3; ++i) {
    full.lifetime[i].reached = true; full.lifetime[i].first.totalSteps = kSpeciesCount * kLegacyEncounterSteps;
    full.lifetime[i].first.bootGeneration = 1;
  }
  full.recentSize = 32;
  for (size_t i = 0; i < 32; ++i) full.recent[i] = full.firstCapture[219 + i];
  CHECK(validate(full)); const auto fullBytes = encode(full);
  CHECK(fullBytes.size() <= codec::maxEncodedSize);
  CHECK(codec::decode(fullBytes.data(), fullBytes.size(), reloaded) == codec::DecodeStatus::Ok);
  CHECK(encode(reloaded) == fullBytes && reloaded.captureCounts[648] == 1 && reloaded.firstCapture[648].species == 649);
  CHECK(captureProbability(2, false, Timing::Normal) == 55 && captureProbability(2, false, Timing::Nice) == 70 &&
        captureProbability(2, false, Timing::Perfect) == 85);
  CHECK(captureProbability(2, true, Timing::Normal) == 30 && captureProbability(2, true, Timing::Nice) == 45 &&
        captureProbability(2, true, Timing::Perfect) == 60);
  std::printf("codec: %u checks passed\n", checks);
}
