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
  uint32_t now() override { return nowMs; }
  uint32_t randomSeed() override { return seed; }
  Sample sample() override {
    Sample s;
    s.fresh = true; s.at = nowMs; s.z = 0.2f;
    return s;
  }
  Input pollInput() override { return Input(); }
  device_clock::Reading readClock() override {
    device_clock::Reading r = {};
    r.valid = true; r.utcSeconds = utc;
    return r;
  }
  device_clock::Reading writeClock(int64_t value) override {
    utc = value;
    device_clock::Reading r = {};
    r.valid = true; r.utcSeconds = utc;
    return r;
  }
  void feedback(uint16_t) override {}
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

void platformBoundary() {
  FakeHost host;
  FakePlatform real;
  TestModePlatform platform(real, host);

  Sample official = platform.sample();
  CHECK(official.fresh && official.z == 0.2f);
  device_clock::Reading r = platform.readClock();
  CHECK(r.utcSeconds == 100000);
  r = platform.writeClock(1234);
  CHECK(real.utc == 1234 && r.utcSeconds == 1234);

  host.state.active = true;
  real.nowMs = 3000;
  r = platform.readClock();
  CHECK(r.valid && r.utcSeconds == 1234);
  real.nowMs = 4500;
  CHECK(platform.readClock().utcSeconds == 1235);

  Sample test = platform.sample();
  CHECK(test.fresh && test.at == 4500 && test.x == 0 && test.y == 0 && test.z == 1.0f);

  r = platform.writeClock(7000);
  CHECK(real.utc == 1234 && r.utcSeconds == 7000);
  CHECK(platform.readClock().utcSeconds == 7000);

  host.state.active = false;
  CHECK(platform.sample().z == 0.2f);
  r = platform.writeClock(4321);
  CHECK(real.utc == 4321 && r.utcSeconds == 4321);
}

}

int main() {
  (void)&seedFor;
  platformBoundary();
  std::printf("PASS test_mode: %u checks\n", checks);
  return 0;
}
