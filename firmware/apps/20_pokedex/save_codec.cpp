#include "save_codec.hpp"
#include "adventure_migration.hpp"
#include "encounter_catalog.hpp"
#include "large_object.hpp"
#include <cstdio>
#include <type_traits>
namespace pokedex { namespace adventure { namespace codec {
namespace {
constexpr uint32_t kMagic = 0x31445641u;
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
template<class A, class T> void clockState(A& a, T& c) {
  a.field(c.calibrated); a.field(c.trusted); a.field(c.anomaly);
  a.field(c.offsetMinutes); a.field(c.dayHighWater); a.field(c.calibrationEpoch);
  reading(a, c.anchor);
  a.field(c.pending.active); a.field(c.pending.transactionId);
  a.field(c.pending.targetUtcSeconds); a.field(c.pending.targetOffsetMinutes);
  a.field(c.pending.targetDayKey); a.field(c.pending.mode);
}
template<class A, class T> void encounterCore(A& a, T& e) {
  a.field(e.id); a.field(e.species); a.field(e.partner); a.field(e.route);
  a.field(e.seed); a.field(e.ruleVersion); a.field(e.usedBalls);
  for (auto& attempt : e.attempts) {
    a.field(attempt.used); a.field(attempt.timing); a.field(attempt.probability);
    a.field(attempt.roll); a.field(attempt.captured); a.field(attempt.ruleVersion);
  }
}
template<class A, class T> void queueEncounter(A& a, T& e) {
  a.field(e.id); a.field(e.species); a.field(e.partner); a.field(e.route);
  a.field(e.seed); a.field(e.ruleVersion); a.field(e.usedBalls); a.field(e.legacyFreeAttempts);
  for (auto& attempt : e.attempts) {
    a.field(attempt.used); a.field(attempt.timing); a.field(attempt.probability);
    a.field(attempt.roll); a.field(attempt.captured); a.field(attempt.ruleVersion);
  }
}
template<class A, class T> void progressionTail(A& a, T& s) {
  a.field(s.rewardBaseSteps); a.field(s.ballsSpent); a.field(s.legacyChargedCount);
  a.field(s.voidedLegacyCount); a.field(s.rosterRevision);
  a.field(s.lastEvolution.request.from); a.field(s.lastEvolution.request.to);
  a.field(s.lastEvolution.request.rosterRevision); a.field(s.lastEvolution.cost);
  stamp(a, s.lastEvolution.at); a.field(s.encounter.legacyFreeAttempts);
}
template<class A, class T> void payloadLegacy(A& a, T& s) {
  const SpeciesCapacity capacity = speciesCapacity(s.schema);
  const size_t bits = capacity.bytes;
  const size_t count = capacity.count;
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
  clockState(a, s.clock);
  a.field(s.bootGeneration); a.field(s.lastStepBatch); a.field(s.nextEncounterId);
  a.field(s.phase); a.field(s.result);
  encounterCore(a, s.encounter);
  if (s.schema >= 2) {
    for (size_t i = 0; i < bits; ++i) a.field(s.registered[i]);
    for (size_t i = 0; i < count; ++i) { a.field(s.progress[i].walked); a.field(s.progress[i].spent); }
    progressionTail(a, s);
  }
  if (s.schema >= 4) { a.field(s.encounterTarget); a.field(s.explorationSpent); }
  if (s.schema >= 8) a.field(s.language);
  if (s.schema >= 9) {
    a.field(s.encounterQueueSize);
    for (auto& e : s.encounterQueue) queueEncounter(a, e);
  }
}
template<class A, class T> void prefixV10(A& a, T& s) {
  a.field(s.starter); a.field(s.partner); a.field(s.route);
  for (size_t i = 0; i < kSpeciesBytes; ++i) a.field(s.owned[i]);
  for (size_t i = 0; i < kSpeciesBytes; ++i) a.field(s.registered[i]);
  a.field(s.totalSteps); a.field(s.explorationCredit); a.field(s.undatedSteps); a.field(s.stepsIncomplete);
  day(a, s.today);
  a.field(s.bestDaySteps);
  for (size_t i = 0; i < 3; ++i) achievement(a, s.dailyFirst[i]);
  for (size_t i = 0; i < 3; ++i) achievement(a, s.lifetime[i]);
  clockState(a, s.clock);
  a.field(s.bootGeneration); a.field(s.lastStepBatch); a.field(s.nextEncounterId);
  a.field(s.phase); a.field(s.result);
  encounterCore(a, s.encounter);
  progressionTail(a, s);
  a.field(s.encounterTarget); a.field(s.explorationSpent);
  a.field(s.language);
}
uint8_t sparseMask(const Save& s, size_t index) {
  uint8_t mask = 0;
  if (s.captureCounts[index] != 0) mask |= 0x01u;
  if (s.firstCapture[index].species != 0) mask |= 0x02u;
  if (s.progress[index].walked != 0 || s.progress[index].spent != 0) mask |= 0x04u;
  return mask;
}
void encodeV10(Writer& a, const Save& s) {
  prefixV10(a, s);
  a.field(s.historySize);
  for (size_t i = 0; i < s.historySize; ++i) day(a, s.history[i]);
  a.field(s.recentSize);
  for (size_t i = 0; i < s.recentSize; ++i) capture(a, s.recent[i]);
  a.field(s.encounterQueueSize);
  for (size_t i = 0; i < s.encounterQueueSize; ++i) queueEncounter(a, s.encounterQueue[i]);
  uint16_t entryCount = 0;
  for (size_t i = 0; i < kSpeciesCount; ++i) {
    if (sparseMask(s, i) != 0) ++entryCount;
  }
  a.field(entryCount);
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    const size_t idx = id - 1;
    const uint8_t mask = sparseMask(s, idx);
    if (!mask) continue;
    a.field(id);
    a.field(mask);
    if (mask & 0x01u) a.field(s.captureCounts[idx]);
    if (mask & 0x02u) {
      const Capture& c = s.firstCapture[idx];
      a.field(c.route);
      stamp(a, c.at);
      a.field(c.achievementMask);
    }
    if (mask & 0x04u) {
      a.field(s.progress[idx].walked);
      a.field(s.progress[idx].spent);
    }
  }
}
void decodeV10(Reader& a, Save& s) {
  prefixV10(a, s);
  if (!a.ok) return;
  a.field(s.historySize);
  if (!a.ok || s.historySize > kHistoryCount) { a.ok = false; return; }
  for (size_t i = 0; i < s.historySize; ++i) day(a, s.history[i]);
  if (!a.ok) return;
  a.field(s.recentSize);
  if (!a.ok || s.recentSize > kRecentCount) { a.ok = false; return; }
  for (size_t i = 0; i < s.recentSize; ++i) capture(a, s.recent[i]);
  if (!a.ok) return;
  a.field(s.encounterQueueSize);
  if (!a.ok || s.encounterQueueSize > s.encounterQueue.size()) { a.ok = false; return; }
  for (size_t i = 0; i < s.encounterQueueSize; ++i) queueEncounter(a, s.encounterQueue[i]);
  if (!a.ok) return;
  uint16_t entryCount = 0;
  a.field(entryCount);
  if (!a.ok || entryCount > kSpeciesCount) { a.ok = false; return; }
  uint16_t prevId = 0;
  for (uint16_t i = 0; i < entryCount; ++i) {
    uint16_t speciesId = 0;
    uint8_t mask = 0;
    a.field(speciesId);
    a.field(mask);
    if (!a.ok || speciesId < 1 || speciesId > kSpeciesCount || speciesId <= prevId ||
        mask == 0 || (mask & ~0x07u) != 0) {
      a.ok = false;
      return;
    }
    prevId = speciesId;
    const size_t idx = speciesId - 1;
    if (mask & 0x01u) {
      a.field(s.captureCounts[idx]);
      if (!a.ok || s.captureCounts[idx] == 0) { a.ok = false; return; }
    }
    if (mask & 0x02u) {
      Capture& c = s.firstCapture[idx];
      c.species = speciesId;
      a.field(c.route);
      stamp(a, c.at);
      a.field(c.achievementMask);
      if (!a.ok) return;
    }
    if (mask & 0x04u) {
      a.field(s.progress[idx].walked);
      a.field(s.progress[idx].spent);
      if (!a.ok || (s.progress[idx].walked == 0 && s.progress[idx].spent == 0)) {
        a.ok = false;
        return;
      }
    }
  }
}
static constexpr uint32_t kCrcNibble[16] = {
  0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
  0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
  0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
  0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu,
};
uint32_t updateCrc(uint32_t crc, const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    crc ^= p[i];
    crc = (crc >> 4) ^ kCrcNibble[crc & 0x0fu];
    crc = (crc >> 4) ^ kCrcNibble[crc & 0x0fu];
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
  Writer data(out + kHeaderSize, capacity - kHeaderSize);
  if (s.schema == 10) encodeV10(data, s);
  else payloadLegacy(data, s);
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
  LargeObject<Save> parsed = makeLargeObject<Save>();
  if (!parsed) return DecodeStatus::IoError;
  parsed->schema = schema; parsed->sequence = sequence;
  Reader data(bytes + kHeaderSize, payloadLength);
  if (parsed->schema == 10) decodeV10(data, *parsed);
  else payloadLegacy(data, *parsed);
  if (!data.ok) return DecodeStatus::Corrupt;
  const bool activityTail = parsed->schema == 3 && data.pos + 22 == payloadLength;
  if (!activityTail && data.pos != payloadLength) return DecodeStatus::Corrupt;
  if (activityTail) {
    if (parsed->encounter.id && !ordinaryEncounter(parsed->encounter.species)) {
      parsed->encounter = Encounter();
      parsed->phase = Phase::Walking;
      parsed->result = Result::None;
    }
    upgradeToSchema4(*parsed);
    upgradeToSchema5(*parsed);
  }
  if (!validate(*parsed)) return DecodeStatus::Corrupt;
  out = *parsed;
  return DecodeStatus::Ok;
}
} } }
