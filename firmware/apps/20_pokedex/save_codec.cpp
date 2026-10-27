#include "save_codec.hpp"
#include "adventure_migration.hpp"
#include "encounter_catalog.hpp"
#include <cstdio>
#include <memory>
#include <new>
#include <type_traits>
namespace pokedex { namespace adventure { namespace codec {
namespace {
constexpr uint32_t kMagic = 0x31445641u; // AVD1 on disk
struct Writer {
  uint8_t* data; size_t capacity; size_t pos; bool ok;
  Writer(uint8_t* p, size_t n) : data(p), capacity(n), pos(0), ok(p != nullptr) {}
  template<class T> void field(const T& value) {
    const uint64_t bits = uint64_t(value);
    const size_t width = std::is_same<T, bool>::value ? 1 : sizeof(T);
    for (size_t i = 0; i < width; ++i) {
      if (pos >= capacity) { ok = false; return; }
      data[pos++] = uint8_t(bits >> (i * 8));
    }
  }
};
struct Reader {
  const uint8_t* data; size_t capacity; size_t pos; bool ok;
  Reader(const uint8_t* p, size_t n) : data(p), capacity(n), pos(0), ok(p != nullptr) {}
  template<class T> void field(T& value) {
    uint64_t bits = 0;
    const size_t width = std::is_same<T, bool>::value ? 1 : sizeof(T);
    for (size_t i = 0; i < width; ++i) {
      if (pos >= capacity) { ok = false; return; }
      bits |= uint64_t(data[pos++]) << (i * 8);
    }
    if (std::is_same<T, bool>::value && bits > 1) ok = false;
    value = static_cast<T>(bits);
  }
};
template<class A, class T> void stamp(A& a, T& s) {
  a.field(s.timeKnown); a.field(s.dayComplete); a.field(s.utcSeconds); a.field(s.offsetMinutes);
  a.field(s.dayKey); a.field(s.bootGeneration); a.field(s.monotonicMs); a.field(s.calibrationEpoch);
  a.field(s.dailySteps); a.field(s.totalSteps);
}
template<class A, class T> void achievement(A& a, T& s) { a.field(s.reached); stamp(a, s.first); }
template<class A, class T> void capture(A& a, T& s) {
  a.field(s.species); a.field(s.route); stamp(a, s.at); a.field(s.achievementMask);
}
template<class A, class T> void day(A& a, T& s) {
  a.field(s.dayKey); a.field(s.epoch); a.field(s.steps); a.field(s.complete);
  for (size_t i = 0; i < 3; ++i) achievement(a, s.milestones[i]);
}
template<class A, class T> void reading(A& a, T& s) {
  a.field(s.trusted); a.field(s.utcSeconds); a.field(s.offsetMinutes); a.field(s.dayKey);
  a.field(s.bootGeneration); a.field(s.monotonicMs); a.field(s.calibrationEpoch);
}
template<class A, class T> void payload(A& a, T& s) {
  // Schema 1/2 keep their frozen 151-species, 19-byte layout. Schema 3 widens to 251/32.
  const size_t bits = s.schema >= 3 ? kSpeciesBytes : 19;
  const size_t count = s.schema >= 3 ? kSpeciesCount : 151;
  a.field(s.starter); a.field(s.partner); a.field(s.route);
  for (size_t i = 0; i < bits; ++i) a.field(s.owned[i]);
  for (size_t i = 0; i < count; ++i) a.field(s.captureCounts[i]);
  for (size_t i = 0; i < count; ++i) capture(a, s.firstCapture[i]);
  a.field(s.totalSteps); a.field(s.explorationCredit); a.field(s.undatedSteps); a.field(s.stepsIncomplete);
  day(a, s.today);
  for (size_t i = 0; i < 14; ++i) day(a, s.history[i]);
  a.field(s.historySize); a.field(s.bestDaySteps);
  for (size_t i = 0; i < 32; ++i) capture(a, s.recent[i]);
  a.field(s.recentSize);
  for (size_t i = 0; i < 3; ++i) achievement(a, s.dailyFirst[i]);
  for (size_t i = 0; i < 3; ++i) achievement(a, s.lifetime[i]);
  a.field(s.clock.calibrated); a.field(s.clock.trusted); a.field(s.clock.anomaly);
  a.field(s.clock.offsetMinutes); a.field(s.clock.dayHighWater); a.field(s.clock.calibrationEpoch);
  reading(a, s.clock.anchor);
  a.field(s.clock.pending.active); a.field(s.clock.pending.transactionId);
  a.field(s.clock.pending.targetUtcSeconds); a.field(s.clock.pending.targetOffsetMinutes);
  a.field(s.clock.pending.targetDayKey); a.field(s.clock.pending.mode);
  a.field(s.bootGeneration); a.field(s.lastStepBatch); a.field(s.nextEncounterId);
  a.field(s.phase); a.field(s.result);
  a.field(s.encounter.id); a.field(s.encounter.species); a.field(s.encounter.partner);
  a.field(s.encounter.route); a.field(s.encounter.seed); a.field(s.encounter.ruleVersion);
  a.field(s.encounter.usedBalls);
  for (size_t i = 0; i < 2; ++i) {
    a.field(s.encounter.attempts[i].used); a.field(s.encounter.attempts[i].timing);
    a.field(s.encounter.attempts[i].probability); a.field(s.encounter.attempts[i].roll);
    a.field(s.encounter.attempts[i].captured); a.field(s.encounter.attempts[i].ruleVersion);
  }
  if (s.schema >= 2) {
    for (size_t i = 0; i < bits; ++i) a.field(s.registered[i]);
    for (size_t i = 0; i < count; ++i) { a.field(s.progress[i].walked); a.field(s.progress[i].spent); }
    a.field(s.rewardBaseSteps); a.field(s.ballsSpent); a.field(s.legacyChargedCount);
    a.field(s.voidedLegacyCount); a.field(s.rosterRevision);
    a.field(s.lastEvolution.request.from); a.field(s.lastEvolution.request.to);
    a.field(s.lastEvolution.request.rosterRevision); a.field(s.lastEvolution.cost);
    stamp(a, s.lastEvolution.at); a.field(s.encounter.legacyFreeAttempts);
  }
  // Schema 4 adds the random encounter cadence account.
  if (s.schema >= 4) { a.field(s.encounterTarget); a.field(s.explorationSpent); }
}
uint32_t updateCrc(uint32_t crc, const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    crc ^= p[i];
    for (unsigned j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}
uint32_t frameCrc(const uint8_t* p, size_t n) {
  return ~updateCrc(updateCrc(0xffffffffu, p, 18), p + kHeaderSize, n - kHeaderSize);
}
}
uint32_t crc32(const uint8_t* p, size_t n) { return ~updateCrc(0xffffffffu, p, n); }
bool encode(const Save& s, uint8_t* out, size_t capacity, size_t& written) {
  written = 0;
  if (!out || capacity < kHeaderSize || !validate(s)) return false;
  Writer data(out + kHeaderSize, capacity - kHeaderSize); payload(data, s);
  if (!data.ok || data.pos + kHeaderSize > maxEncodedSize) return false;
  Writer header(out, kHeaderSize);
  header.field(kMagic); header.field(s.schema); header.field(s.sequence);
  header.field(uint32_t(data.pos)); header.field(uint32_t(0));
  written = kHeaderSize + data.pos;
  const uint32_t crc = frameCrc(out, written);
  Writer checksum(out + 18, 4); checksum.field(crc);
  return true;
}
DecodeStatus decode(const uint8_t* bytes, size_t length, Save& out) {
  if (!bytes || length < kHeaderSize) return DecodeStatus::Corrupt;
  Reader header(bytes, kHeaderSize);
  uint32_t magic = 0, payloadLength = 0, crc = 0; uint16_t schema = 0; uint64_t sequence = 0;
  header.field(magic); header.field(schema); header.field(sequence); header.field(payloadLength); header.field(crc);
  if (magic != kMagic) return DecodeStatus::Corrupt;
  if (schema < 1 || schema > kSchemaVersion) return DecodeStatus::UnsupportedVersion;
  if (length > maxEncodedSize) return DecodeStatus::Corrupt;
  if (payloadLength != length - kHeaderSize || crc != frameCrc(bytes, length)) return DecodeStatus::Corrupt;
  // A Save exceeds a typical embedded task stack. Decode transactionally on the heap.
  std::unique_ptr<Save> parsed(new (std::nothrow) Save());
  if (!parsed) return DecodeStatus::IoError;
  parsed->schema = schema; parsed->sequence = sequence;
  Reader data(bytes + kHeaderSize, payloadLength); payload(data, *parsed);
  if (!data.ok) return DecodeStatus::Corrupt;
  // Frames written by the unmerged activity-branch firmware (legendary-ruins,
  // dragon-trial) append a 22-byte activity block to an otherwise ordinary
  // schema-3 payload. Skip that block, retire any in-flight activity encounter
  // (its species lies outside the ordinary pool), and upgrade to schema 4 on
  // the spot: schema-3 bookkeeping cannot express encounters that consumed no
  // exploration credit, while schema-4 accepts the zero-cost id range.
  const bool activityTail = parsed->schema == 3 && data.pos + 22 == payloadLength;
  if (!activityTail && data.pos != payloadLength) return DecodeStatus::Corrupt;
  if (activityTail) {
    if (parsed->encounter.id && !ordinaryEncounter(parsed->encounter.species)) {
      parsed->encounter = Encounter();
      parsed->phase = Phase::Walking;
      parsed->result = Result::None;
    }
    upgradeToSchema4(*parsed);
  }
  if (!validate(*parsed)) return DecodeStatus::Corrupt;
  out = *parsed;
  return DecodeStatus::Ok;
}
} } }
