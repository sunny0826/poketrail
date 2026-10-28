#include "support/adventure_test/harness.hpp"
#include "test_mode_platform.hpp"
#include <cstdint>

using namespace pokedex;
using namespace pokedex::adventure;
using namespace pokedex::ui;

namespace {
class FakePlatform : public Platform {
 public:
  uint32_t nowMs = 0, seed = 7;
  int64_t utc = 100000;
  unsigned samples = 0, clockReads = 0, clockWrites = 0, feedbacks = 0;
  uint32_t now() override { return nowMs; }
  uint32_t randomSeed() override { return seed; }
  Sample sample() override {
    ++samples;
    Sample s;
    s.fresh = true; s.at = nowMs; s.z = 0.2f;
    return s;
  }
  Input pollInput() override { return Input(); }
  device_clock::Reading readClock() override {
    ++clockReads;
    device_clock::Reading r = {};
    r.valid = true; r.utcSeconds = utc;
    return r;
  }
  device_clock::Reading writeClock(int64_t value) override {
    ++clockWrites; utc = value;
    device_clock::Reading r = {};
    r.valid = true; r.utcSeconds = utc;
    return r;
  }
  void feedback(uint16_t) override { ++feedbacks; }
  bool vibrationActive() override { return false; }
  void brightness(uint8_t) override {}
  c152::DisplayTransition displaySleep(bool) override { return c152::DisplayTransition(true); }
  c152::BatteryStatus readBattery() override { return c152::BatteryStatus(); }
};

struct FakeHost : TestModeHost {
  TestMode state;
  bool testMode() const override { return state.active; }
  bool ramOnly() const override { return state.active && state.ramOnly; }
  bool enterTest(TestSeed seed) override {
    state.active = true;
    state.ramOnly = seed == TestSeed::Blank;
    return true;
  }
  bool leaveTest() override { state.active = false; return true; }
  TestMode& mode() override { return state; }
  void setFault(TestFault fault) override { state.fault = fault; }
  TestFault fault() const override { return state.fault; }
  TestDiagnostics diagnostics() const override { return TestDiagnostics(); }
};

void simulatedClock() {
  TestMode mode;
  CHECK(!mode.clockStarted && mode.utcAt(1234) == 0);
  mode.beginSimulation(1000, 50000);
  CHECK(mode.clockStarted);
  CHECK(mode.utcAt(1000) == 50000);
  CHECK(mode.utcAt(3500) == 50002);
  CHECK(mode.utcAt(999) == 50000);  // sub-second wrap stays at the base second
  mode.setUtc(4000, 60000);
  CHECK(mode.utcAt(4000) == 60000 && mode.utcAt(5000) == 60001);
  mode.setOffset(60);
  CHECK(mode.clockOffsetMinutes == 60);
  mode.endSimulation();
  CHECK(!mode.clockStarted && mode.utcAt(4000) == 0);
}

void platformBoundary() {
  FakeHost host;
  FakePlatform real;
  TestModePlatform platform(real, host);

  CHECK(platform.now() == 0 && platform.randomSeed() == 7);
  Sample official = platform.sample();
  CHECK(real.samples == 1 && official.fresh && official.z == 0.2f);
  device_clock::Reading r = platform.readClock();
  CHECK(real.clockReads == 1 && r.utcSeconds == 100000);
  r = platform.writeClock(1234);
  CHECK(real.clockWrites == 1 && real.utc == 1234 && r.utcSeconds == 1234);

  host.state.active = true;
  real.nowMs = 3000;
  r = platform.readClock();
  CHECK(real.clockReads == 2 && host.state.clockStarted);
  CHECK(r.valid && r.utcSeconds == 1234);
  real.nowMs = 4500;
  CHECK(platform.readClock().utcSeconds == 1235);

  const unsigned imuCalls = real.samples;
  Sample test = platform.sample();
  CHECK(real.samples == imuCalls);  // real IMU is never polled in test mode
  CHECK(test.fresh && test.at == 4500 && test.x == 0 && test.y == 0 && test.z == 1.0f);

  const unsigned rtcWrites = real.clockWrites;
  r = platform.writeClock(7000);
  CHECK(real.clockWrites == rtcWrites && r.utcSeconds == 7000);
  CHECK(platform.readClock().utcSeconds == 7000);

  host.state.active = false;
  CHECK(platform.sample().z == 0.2f && real.samples == imuCalls + 1);
  r = platform.writeClock(4321);
  CHECK(real.clockWrites == rtcWrites + 1 && real.utc == 4321 && r.utcSeconds == 4321);
}

void stepInjectionPath() {
  MemoryStore store;
  Save initial;
  initial.sequence = 1;
  initial.encounterTarget = 300;
  store.disk = bytes(initial);
  Engine engine(store);
  CHECK(engine.begin() == CommandResult::Applied);
  CHECK(engine.chooseStarter(1) == CommandResult::Applied);
  const uint16_t target = engine.view().encounterTarget;
  CHECK(target >= kEncounterMinSteps && target <= kEncounterMaxSteps);
  uint32_t now = 1000;
  for (uint32_t i = 0; i < target; ++i) {
    now += 500;
    StepEvent event;
    event.batchId = engine.view().lastStepBatch + 1;
    event.clock = reading(engine, 1792627200LL + now / 1000, now);
    event.partner = engine.view().partner;
    CHECK(engine.observeSteps(&event, 1, 1) == CommandResult::Applied);
  }
  CHECK(engine.view().phase == Phase::EncounterReady);
  CHECK(engine.view().totalSteps >= target);
}

void noBootWriteOnModeExit() {
  MemoryStore store;
  Engine first(store);
  CHECK(first.begin() == CommandResult::Applied);
  CHECK(first.chooseStarter(1) == CommandResult::Applied);
  CHECK(first.checkpoint() == CommandResult::Applied || first.checkpoint() == CommandResult::Duplicate);
  const auto disk = store.disk;
  const unsigned commits = store.commits;
  Engine reloaded(store);
  CHECK(reloaded.begin(false) == CommandResult::Applied);
  CHECK(reloaded.ready());
  CHECK(store.disk == disk && store.commits == commits);
  CHECK(reloaded.view().totalSteps == first.view().totalSteps &&
        reloaded.view().starter == first.view().starter);
}
}

int main() {
  (void)&seedFor;
  simulatedClock();
  platformBoundary();
  stepInjectionPath();
  noBootWriteOnModeExit();
  std::printf("PASS test_mode: %u checks\n", checks);
  return 0;
}
