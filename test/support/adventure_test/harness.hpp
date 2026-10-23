#pragma once
#include "adventure.hpp"
#include "encounter_catalog.hpp"
#include "save_codec.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
using namespace pokedex::adventure;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static std::vector<uint8_t> bytes(const Save& s) {
  std::vector<uint8_t> out(codec::maxEncodedSize); size_t n = 0;
  CHECK(codec::encode(s, out.data(), out.size(), n)); out.resize(n); return out;
}
struct MemoryStore : Store {
  std::vector<uint8_t> disk, lastAttempt;
  bool failBefore = false, failAfter = false;
  unsigned commits = 0, failAt = 0;
  LoadStatus forced = LoadStatus::Loaded;
  LoadResult load(Save& out) override {
    if (forced != LoadStatus::Loaded) return LoadResult(forced);
    if (disk.empty()) return LoadResult(LoadStatus::Empty);
    return LoadResult(codec::decode(disk.data(), disk.size(), out) == codec::DecodeStatus::Ok ? LoadStatus::Loaded : LoadStatus::Corrupt);
  }
  CommitStatus commit(const Save& s) override {
    ++commits; lastAttempt = bytes(s);
    if (failBefore || commits == failAt) return CommitStatus::IoError;
    if (!disk.empty()) {
      Save old; CHECK(codec::decode(disk.data(), disk.size(), old) == codec::DecodeStatus::Ok);
      if (old.sequence > s.sequence || (old.sequence == s.sequence && disk != lastAttempt)) return CommitStatus::Conflict;
    }
    disk = lastAttempt;
    return failAfter ? CommitStatus::IoError : CommitStatus::Committed;
  }
};
static ClockReading reading(const Engine& e, int64_t utc, uint32_t ms, bool trusted = true, int16_t offset = 480) {
  ClockReading r; r.trusted = trusted; r.utcSeconds = utc; r.monotonicMs = ms; r.offsetMinutes = offset;
  r.bootGeneration = e.view().bootGeneration; r.calibrationEpoch = e.view().clock.calibrationEpoch;
  time_t local = time_t(utc + int64_t(offset) * 60); const std::tm* t = std::gmtime(&local);
  r.dayKey = uint32_t((t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday);
  return r;
}
struct World {
  MemoryStore store; Engine e; uint32_t now = 1000; int64_t base = 1792627200LL;
  World(bool calibrated = false) : e(store) {
    CHECK(e.begin() == CommandResult::Applied);
    CHECK(e.chooseStarter(1) == CommandResult::Applied);
    if (calibrated) calibrate();
  }
  ClockReading clock(bool trusted = true) {
    return reading(e, base + now / 1000, now, trusted, e.view().clock.calibrated ? e.view().clock.offsetMinutes : 480);
  }
  void calibrate(ClockChangeMode mode = ClockChangeMode::Normal, int16_t offset = 480) {
    ClockReading r = clock(); r.offsetMinutes = offset;
    r = reading(e, r.utcSeconds, now, true, offset);
    const uint64_t id = e.view().clock.pending.transactionId + 1;
    CHECK(e.beginClockChange(id, mode, r) == CommandResult::Applied);
    CHECK(e.finishClockChange(id, r) == CommandResult::Applied);
  }
  void steps(unsigned count, uint32_t seed = 1, bool trusted = true) {
    for (unsigned i = 0; i < count; ++i) {
      now += 500; StepEvent event; event.batchId = e.view().lastStepBatch + 1; event.clock = clock(trusted); event.partner = e.view().partner;
      CHECK(e.observeSteps(&event, 1, seed) == CommandResult::Applied);
    }
  }
  void win() {
    CHECK(e.startBattle(now) == CommandResult::Applied);
    while (e.view().phase == Phase::Battle) {
      now = e.battleView().targetMs;
      CHECK(e.press(e.battleView().expected, now) == CommandResult::Applied);
      if (e.view().phase == Phase::Battle) {
        now += 181; CHECK(e.tick(now, clock()) == CommandResult::Applied);
      }
    }
    CHECK(e.view().phase == Phase::ThrowReady);
  }
  CommandResult ball(uint8_t index, Timing timing = Timing::Perfect) {
    AttemptKey key(e.view().encounter.id, index);
    CHECK(e.beginThrow(key, now) == CommandResult::Applied);
    now = e.throwView().targetMs + (timing == Timing::Miss ? 300 : timing == Timing::Normal ? 150 : timing == Timing::Nice ? 100 : 0);
    return e.pressThrow(key, now, clock());
  }
};
static uint32_t seedFor(const Save& s, bool first, bool second, uint16_t species = 0) {
  for (uint32_t seed = 0; seed < 1000000; ++seed) {
    Encounter e; e.seed = seed; e.id = s.nextEncounterId; e.species = chooseEncounter(s, s.route, seed);
    if (species && species != e.species) continue;
    const uint8_t p = captureProbability(catalogEntry(e.species)->rare, Timing::Perfect);
    if ((throwRoll(e, 1) < p) == first && (throwRoll(e, 2) < p) == second) return seed;
  }
  CHECK(false); return 0;
}
