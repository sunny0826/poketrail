#include "adventure.hpp"
#include "calendar.hpp"
#include "encounter_catalog.hpp"
#include "file_store.hpp"
#include "save_codec.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
using namespace pokedex::adventure;
namespace {
unsigned checks = 0, failures = 0;
#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr, "setup line %d: %s\n", __LINE__, #x); std::exit(2); } } while (0)
void expect(bool ok, const char* message) {
  ++checks;
  if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
std::vector<uint8_t> encode(const Save& save) {
  std::vector<uint8_t> bytes(codec::maxEncodedSize); size_t size = 0;
  REQUIRE(codec::encode(save, bytes.data(), bytes.size(), size));
  bytes.resize(size); return bytes;
}
struct Slots : SlotIO {
  std::vector<uint8_t> bytes[2];
  LoadStatus prepare() override { return LoadStatus::Loaded; }
  ReadStatus read(unsigned slot, uint8_t* out, size_t capacity, size_t& size) override {
    size = bytes[slot].size();
    if (!size) return ReadStatus::Missing;
    std::copy(bytes[slot].begin(), bytes[slot].begin() + std::min(size, capacity), out);
    return size > capacity ? ReadStatus::TooLarge : ReadStatus::Ok;
  }
  bool write(unsigned slot, const uint8_t* input, size_t size) override {
    bytes[slot].assign(input, input + size); return true;
  }
};
struct World {
  Slots slots; FileStore store; Engine engine;
  uint64_t batch = 0;
  World() : store(slots), engine(store) {
    REQUIRE(engine.begin() == CommandResult::Applied);
    REQUIRE(engine.chooseStarter(1) == CommandResult::Applied);
  }
  ClockReading reading(int64_t utc, uint32_t ms, int16_t offset = 0) const {
    ClockReading r; r.trusted = true; r.utcSeconds = utc; r.monotonicMs = ms;
    r.offsetMinutes = offset; r.bootGeneration = engine.view().bootGeneration;
    r.calibrationEpoch = engine.view().clock.calibrationEpoch;
    pokedex::calendar::CivilTime local;
    REQUIRE(pokedex::calendar::fromUtc(utc, offset, local));
    REQUIRE(pokedex::calendar::dayKey(local, r.dayKey)); return r;
  }
  void calibrate(const ClockReading& r, ClockChangeMode mode = ClockChangeMode::Normal) {
    const uint64_t id = engine.view().clock.pending.transactionId + 1;
    REQUIRE(engine.beginClockChange(id, mode, r) == CommandResult::Applied);
    REQUIRE(engine.finishClockChange(id, r) == CommandResult::Applied);
  }
  void step(const ClockReading& r, uint32_t seed = 1) {
    StepEvent event; event.batchId = ++batch; event.clock = r;
    REQUIRE(engine.observeSteps(&event, 1, seed) == CommandResult::Applied);
  }
};
void captureFixture(World& w, int64_t utc) {
  uint32_t seed = 0;
  for (; seed < 1000000; ++seed) {
    Encounter e; e.id = w.engine.view().nextEncounterId; e.seed = seed;
    e.species = chooseEncounter(w.engine.view(), w.engine.view().route, seed);
    if (throwRoll(e, 1) < captureProbability(catalogEntry(e.species)->rare, Timing::Perfect)) break;
  }
  REQUIRE(seed < 1000000);
  uint32_t ms = 1000;
  for (unsigned i = 0; i < 5000; ++i) {
    ms += 500; w.step(w.reading(utc + (ms - 1000) / 1000, ms), seed);
  }
  REQUIRE(w.engine.startBattle(ms) == CommandResult::Applied);
  while (w.engine.view().phase == Phase::Battle) {
    ms = w.engine.battleView().targetMs;
    REQUIRE(w.engine.press(w.engine.battleView().expected, ms) == CommandResult::Applied);
    if (w.engine.view().phase == Phase::Battle) {
      ms += 181;
      REQUIRE(w.engine.tick(ms, w.reading(utc + (ms - 1000) / 1000, ms)) == CommandResult::Applied);
    }
  }
  REQUIRE(w.engine.view().phase == Phase::ThrowReady);
  const AttemptKey key(w.engine.view().encounter.id, 1);
  REQUIRE(w.engine.beginThrow(key, ms) == CommandResult::Applied);
  ms = w.engine.throwView().targetMs;
  REQUIRE(w.engine.pressThrow(key, ms, w.reading(utc + (ms - 1000) / 1000, ms)) == CommandResult::Applied);
  REQUIRE(w.engine.view().recentSize == 1 && w.engine.view().recent[0].at.timeKnown);
}
void lateAchievement(unsigned threshold, bool lifetime) {
  std::unique_ptr<World> w(new World);
  const int64_t midnight = 1792713600LL;
  const int64_t base = midnight - int64_t(threshold / 2) - 1;
  w->calibrate(w->reading(base, 0));
  for (unsigned i = 1; i < threshold; ++i) w->step(w->reading(base + i / 2, i * 500));
  const ClockReading late = w->reading(midnight - 1, threshold * 500);
  REQUIRE(w->engine.observeClock(w->reading(midnight + 1, threshold * 500 + 2000)) == CommandResult::Applied);
  w->step(late);
  const Save& s = w->engine.view();
  const Stamp& stamp = lifetime ? s.lifetime[0].first : s.dailyFirst[0].first;
  expect(s.today.steps == 0 && s.history[0].steps == threshold, "late peak goes to its historical bucket");
  expect(stamp.timeKnown && stamp.utcSeconds == midnight - 1 && stamp.dayKey == 20261022 &&
         stamp.dailySteps == threshold && stamp.totalSteps == threshold,
         "late milestone retains its actual date and historical step count");
  FileStore reloadStore(w->slots); std::unique_ptr<Engine> reload(new Engine(reloadStore));
  REQUIRE(reload->begin() == CommandResult::Applied);
  expect((lifetime ? reload->view().lifetime[0] : reload->view().dailyFirst[0]).first.timeKnown,
         "known milestone date survives a real codec and FileStore reload");
}
void rebootSameDay() {
  std::unique_ptr<World> w(new World);
  const int64_t utc = 1792886400LL;
  w->calibrate(w->reading(utc, 1000));
  w->step(w->reading(utc + 60, 61000));
  Save restarted = w->engine.view();
  restarted.clock.trusted = false;
  restarted.clock.anchor = ClockReading();
  ++restarted.bootGeneration;
  restarted.lastStepBatch = 0;
  Slots slots;
  slots.bytes[0] = encode(w->engine.view());
  ++restarted.sequence;
  slots.bytes[1] = encode(restarted);
  FileStore store(slots);
  std::unique_ptr<Engine> engine(new Engine(store));
  REQUIRE(engine->begin() == CommandResult::Applied);
  ClockReading first = w->reading(utc + 120, 1000);
  first.bootGeneration = engine->view().bootGeneration;
  first.calibrationEpoch = engine->view().clock.calibrationEpoch;
  pokedex::calendar::CivilTime local;
  REQUIRE(pokedex::calendar::fromUtc(first.utcSeconds, first.offsetMinutes, local));
  REQUIRE(pokedex::calendar::dayKey(local, first.dayKey));
  REQUIRE(engine->observeClock(first) == CommandResult::Applied);
  StepEvent event;
  event.batchId = 1;
  event.clock = first;
  event.clock.utcSeconds = utc + 180;
  event.clock.monotonicMs = 61000;
  REQUIRE(engine->observeSteps(&event, 1, 1) == CommandResult::Applied);
  const Save& save = engine->view();
  expect(save.clock.trusted && !save.clock.anomaly && save.today.dayKey == 20261025 &&
         save.today.steps == 2 && save.undatedSteps == 0 && save.totalSteps == 2,
         "reboot keeps the same local day and counts its steps as today");
  ClockReading backward = first;
  backward.utcSeconds = utc - 86400;
  backward.monotonicMs = 70000;
  REQUIRE(pokedex::calendar::fromUtc(backward.utcSeconds, backward.offsetMinutes, local));
  REQUIRE(pokedex::calendar::dayKey(local, backward.dayKey));
  REQUIRE(engine->observeClock(backward) == CommandResult::Applied);
  expect(!engine->view().clock.trusted && engine->view().clock.anomaly &&
         engine->view().today.dayKey == 20261025 && engine->view().today.steps == 2,
         "a real backward date remains an anomaly and keeps the current day");
  ClockReading repaired = first;
  repaired.monotonicMs = 80000;
  const uint64_t id = engine->view().clock.pending.transactionId + 1;
  REQUIRE(engine->beginClockChange(id, ClockChangeMode::Normal, repaired) == CommandResult::Applied);
  REQUIRE(engine->finishClockChange(id, repaired) == CommandResult::Applied);
  const Save& recovered = engine->view();
  expect(recovered.clock.trusted && !recovered.clock.anomaly && recovered.today.dayKey == 20261025 &&
         recovered.today.steps == 2,
         "normal recalibration clears a saved anomaly and restores today");
  ClockReading older = repaired;
  older.utcSeconds = utc - 2 * 86400;
  older.monotonicMs = 90000;
  REQUIRE(pokedex::calendar::fromUtc(older.utcSeconds, older.offsetMinutes, local));
  REQUIRE(pokedex::calendar::dayKey(local, older.dayKey));
  const uint64_t repair = recovered.clock.pending.transactionId + 1;
  const uint32_t epoch = engine->view().clock.calibrationEpoch;
  REQUIRE(engine->beginClockChange(repair, ClockChangeMode::Normal, older) == CommandResult::Applied);
  REQUIRE(engine->finishClockChange(repair, older) == CommandResult::Applied);
  expect(engine->view().clock.trusted && !engine->view().clock.anomaly &&
         engine->view().clock.calibrationEpoch == epoch + 1 &&
         engine->view().today.dayKey == older.dayKey && engine->view().today.steps == 0,
         "normal recalibration to an earlier date starts a new calibration and archives the old day");
}
void clockEdges() {
  for (int drift : {120000, 120001, -120000, -120001}) {
    std::unique_ptr<World> w(new World);
    const int64_t utc = 1792713599LL;
    w->calibrate(w->reading(utc, 1000));
    const uint32_t ms = uint32_t(2000 + (drift > 120000 ? -1 : drift < -120000 ? 1 : 0));
    w->step(w->reading(utc + (drift > 0 ? 121 : -119), ms));
    const Save& s = w->engine.view();
    const bool abnormal = drift > 120000 || drift < -120000;
    expect(s.clock.anomaly == abnormal, "step timestamp checks the inclusive 120-second drift limit");
    expect(s.totalSteps == 1 && s.explorationCredit == 1, "anomalous time preserves cumulative steps and credit");
    if (abnormal) expect(s.clock.dayHighWater == 20261022 && s.undatedSteps == 1,
                        "anomalous time cannot advance the day watermark");
  }
  std::unique_ptr<World> w(new World);
  w->calibrate(w->reading(1792713599LL, UINT32_MAX - 499));
  const auto next = w->reading(1792713600LL, 500);
  w->step(next); REQUIRE(w->engine.observeClock(next) == CommandResult::Applied);
  w->step(w->reading(1792713599LL, UINT32_MAX - 499));
  expect(w->engine.view().today.steps == 1 && w->engine.view().history[0].steps == 1 &&
         !w->engine.view().clock.anomaly, "monotonic wrap accepts both forward and late peaks");
}
void historicalBoundaries() {
  std::unique_ptr<World> w(new World);
  const int64_t utc = 1792627200LL;
  w->calibrate(w->reading(utc, 1000));
  const ClockReading old = w->reading(utc, 1000);
  w->step(old);
  for (unsigned day = 1; day <= 15; ++day) {
    const auto current = w->reading(utc + int64_t(day) * 86400, 1000 + day * 86400000u);
    REQUIRE(w->engine.observeClock(current) == CommandResult::Applied);
  }
  w->step(old);
  expect(w->engine.view().undatedSteps == 1 && !makeStamp(w->engine.view(), old).timeKnown,
         "an evicted historical day remains unknown rather than gaining a replacement date");
  const auto beforeRepair = w->reading(utc + 15 * 86400LL, 1000 + 15 * 86400000u);
  w->calibrate(beforeRepair, ClockChangeMode::RepairWrongDate);
  w->step(beforeRepair);
  expect(w->engine.view().undatedSteps == 2 && !makeStamp(w->engine.view(), beforeRepair).timeKnown,
         "a peak from a previous calibration epoch stays undated");
}
void dateCorruption() {
  std::unique_ptr<World> w(new World);
  w->calibrate(w->reading(1792627200LL, 1000));
  captureFixture(*w, 1792627200LL);
  auto positive = w->reading(1792713599LL, 1000, 840);
  expect(validReading(positive) && positive.dayKey == 20261023, "positive timezone derives the next local day");
  positive.dayKey = 20261022;
  expect(!validReading(positive), "trusted timestamps reject contradictory local dates");
  auto negative = w->reading(1792627200LL, 1000, -720);
  expect(validReading(negative) && negative.dayKey == 20261021, "negative timezone derives the previous day");
  std::unique_ptr<Save> next(new Save(w->engine.view())); ++next->sequence;
  const auto good = encode(*next); unsigned changed = 0;
  // Locate known UTC/offset/day tuples; mutate bytes after encoding and preserve CRC validity.
  for (size_t pos = 22; pos + 14 <= good.size(); ++pos) {
    uint64_t utc = 0; for (unsigned i = 0; i < 8; ++i) utc |= uint64_t(good[pos + i]) << (8 * i);
    uint32_t day = 0; for (unsigned i = 0; i < 4; ++i) day |= uint32_t(good[pos + 10 + i]) << (8 * i);
    if (utc < 1792627200ULL || utc >= 1792713600ULL || good[pos + 8] || good[pos + 9] || day != 20261022) continue;
    ++changed; Slots slots; slots.bytes[0] = encode(w->engine.view()); slots.bytes[1] = good;
    auto& damaged = slots.bytes[1];
    for (unsigned i = 0; i < 4; ++i) damaged[pos + 10 + i] = uint8_t(20990101u >> (8 * i));
    std::vector<uint8_t> checked(damaged.begin(), damaged.begin() + 18);
    checked.insert(checked.end(), damaged.begin() + 22, damaged.end());
    const uint32_t crc = codec::crc32(checked.data(), checked.size());
    for (unsigned i = 0; i < 4; ++i) damaged[18 + i] = uint8_t(crc >> (8 * i));
    std::unique_ptr<Save> out(new Save); out->sequence = 777;
    expect(codec::decode(damaged.data(), damaged.size(), *out) == codec::DecodeStatus::Corrupt && out->sequence == 777,
           "CRC-correct contradictory dates are rejected without publishing a partial save");
    FileStore store(slots); const auto result = store.load(*out);
    expect(result.status == LoadStatus::Loaded && result.degraded && out->sequence == w->engine.view().sequence,
           "semantic corruption falls back to the older valid slot");
  }
  std::printf("date tuples tested: %u\n", changed);
  expect(changed == 10, "first/recent capture, six milestone, anchor and clock-transaction dates were checked");
}
struct RetryStore : Store {
  unsigned loads = 0, commits = 0; bool fail = true;
  LoadResult load(Save& s) override { ++loads; s.sequence = 900; s.totalSteps = 123; return LoadResult(LoadStatus::Empty); }
  CommitStatus commit(const Save& s) override {
    ++commits;
    REQUIRE(s.sequence == 1 && s.totalSteps == 0 && s.nextEncounterId == 1 && s.bootGeneration == 1);
    return fail ? CommitStatus::IoError : CommitStatus::Committed;
  }
};
void bootRetry() {
  RetryStore store; std::unique_ptr<Engine> engine(new Engine(store));
  REQUIRE(engine->begin() == CommandResult::StorageError);
  REQUIRE(engine->begin() == CommandResult::SavePending && store.loads == 1);
  store.fail = false; REQUIRE(engine->retrySave() == CommandResult::Applied);
  expect(engine->ready() && store.loads == 1 && store.commits == 2 && engine->begin() == CommandResult::Duplicate,
         "Empty startup restores defaults and retries the identical boot candidate");
}
}
int main() {
  lateAchievement(1000, false); lateAchievement(10000, true);
  rebootSameDay(); clockEdges(); historicalBoundaries(); dateCorruption(); bootRetry();
  std::printf("adventure regressions: %u checks, %u failures\n", checks, failures);
  return failures ? 1 : 0;
}
