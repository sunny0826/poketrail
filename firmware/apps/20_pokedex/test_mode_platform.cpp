#include "test_mode_platform.hpp"

namespace pokedex { namespace ui {
using namespace adventure;

TestModePlatform::TestModePlatform(Platform& real, TestModeHost& host)
    : real_(real), host_(host) {}

void TestModePlatform::ensureSimulation() {
  if (!host_.testMode()) return;
  TestMode& mode = host_.mode();
  if (mode.clockStarted) return;
  // Reading the hardware clock is safe; only writes are suppressed in tests.
  const device_clock::Reading base = real_.readClock();
  mode.beginSimulation(real_.now(), base.valid ? base.utcSeconds : kTestDefaultUtc);
}

uint32_t TestModePlatform::now() { return real_.now(); }
uint32_t TestModePlatform::randomSeed() { return real_.randomSeed(); }

Sample TestModePlatform::sample() {
  if (!host_.testMode()) return real_.sample();
  // A stationary 1g sample keeps the detector cadence valid while producing no
  // steps. The real IMU is never polled in test mode.
  Sample s;
  s.fresh = true;
  s.at = real_.now();
  s.z = 1.0f;
  return s;
}

Input TestModePlatform::pollInput() { return real_.pollInput(); }

device_clock::Reading TestModePlatform::readClock() {
  if (!host_.testMode()) return real_.readClock();
  ensureSimulation();
  device_clock::Reading r = {};
  r.utcSeconds = host_.mode().utcAt(real_.now());
  r.valid = true;
  return r;
}

device_clock::Reading TestModePlatform::writeClock(int64_t utc) {
  if (!host_.testMode()) return real_.writeClock(utc);
  ensureSimulation();
  host_.mode().setUtc(real_.now(), utc);
  return readClock();
}

void TestModePlatform::feedback(uint16_t duration) { real_.feedback(duration); }
bool TestModePlatform::vibrationActive() { return real_.vibrationActive(); }
bool TestModePlatform::usbHostPresent() { return real_.usbHostPresent(); }
void TestModePlatform::brightness(uint8_t value) { real_.brightness(value); }
c152::DisplayTransition TestModePlatform::displaySleep(bool sleeping) { return real_.displaySleep(sleeping); }
c152::BatteryStatus TestModePlatform::readBattery() { return real_.readBattery(); }

} }
