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
int main() {
  Save s; s.sequence = 0x0102030405060708ULL; s.bootGeneration = 1;
  CHECK(validate(s)); const std::vector<uint8_t> original = encode(s);
  CHECK(original[0] == 0x41 && original[1] == 0x56 && original[2] == 0x44 && original[3] == 0x31);
  CHECK(original[6] == 8 && original[13] == 1);
  CHECK(codec::crc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926u);
  Save decoded; CHECK(codec::decode(original.data(), original.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(encode(decoded) == original && decoded.sequence == s.sequence);
  for (size_t n = 0; n < original.size(); ++n)
    CHECK(codec::decode(original.data(), n, decoded) == codec::DecodeStatus::Corrupt);
  CHECK(codec::decode(nullptr, original.size(), decoded) == codec::DecodeStatus::Corrupt);
  std::vector<uint8_t> data = original; data.push_back(0);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  data.resize(codec::maxEncodedSize + 1);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  data = original; data[4] = 3;
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::UnsupportedVersion);
  data.resize(codec::maxEncodedSize + 100);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::UnsupportedVersion);
  for (size_t i = 0; i < original.size(); ++i) {
    if (i == 4 || i == 5) continue;
    data = original; data[i] ^= 0x80;
    CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  }
  CHECK(encode(decoded) == original); // all failures preserve the destination
  data = original; data[22 + 4] = 3; checksum(data);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  data = original; data[7922] = 2; checksum(data); // bool stepsIncomplete must be 0/1
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  data = original; data[12139 - 36] = 4; checksum(data);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  std::vector<uint8_t> small(original.size() - 1); size_t n = 12;
  CHECK(!codec::encode(s, small.data(), small.size(), n) && n == 0);
  CHECK(!codec::encode(s, nullptr, 0, n));
  Save bad = s; bad.route = Route(3); reject(bad);
  bad = s; bad.starter = 2; reject(bad);
  bad = s; bad.partner = 151; reject(bad);
  bad = s; bad.owned[18] = 0x80; reject(bad);
  bad = s; bad.historySize = 15; reject(bad);
  bad = s; bad.recentSize = 33; reject(bad);
  bad = s; bad.phase = Phase::ThrowReady; reject(bad);
  bad = s; bad.encounter.usedBalls = 3; reject(bad);
  bad = s; bad.captureCounts[0] = 1; reject(bad);
  bad = s; bad.clock.pending.active = true; reject(bad);
  bad = s; bad.clock.trusted = true; reject(bad);
  bad = s; bad.today.dayKey = 20260229; reject(bad);
  bad = s; bad.clock.offsetMinutes = 841; reject(bad);
  bad = s; bad.explorationCredit = 1; reject(bad);
  bad = s; bad.firstCapture[0].at.bootGeneration = 2; reject(bad);
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
  CHECK(!catalogEntry(0) && !catalogEntry(152));
  unsigned routes[3] = {0,0,0};
  for (uint16_t id = 1; id <= 151; ++id) {
    const CatalogEntry* e = catalogEntry(id); CHECK(e && e->weight > 0 && e->routes > 0 && e->routes <= 7);
    for (unsigned r = 0; r < 3; ++r) if (e->routes & (1u << r)) ++routes[r];
  }
  CHECK(routes[0] && routes[1] && routes[2]);
  CHECK(captureProbability(false, Timing::Normal) == 55 && captureProbability(false, Timing::Nice) == 70 && captureProbability(false, Timing::Perfect) == 85);
  CHECK(captureProbability(true, Timing::Normal) == 30 && captureProbability(true, Timing::Nice) == 45 && captureProbability(true, Timing::Perfect) == 60);
  CHECK(captureProbability(true, Timing::Miss) == 0);
  std::printf("codec: %u checks passed; encoded=%zu, max=%zu, two slots=%zu bytes; pools=%u/%u/%u\n",
              checks, data.size(), codec::maxEncodedSize, data.size() * 2, routes[0], routes[1], routes[2]);
}
