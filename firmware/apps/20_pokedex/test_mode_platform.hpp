#pragma once
#include "runtime.hpp"
#include "test_mode.hpp"

namespace pokedex { namespace ui {

// Decorates the real Platform for a test session: simulated clock, synthetic
// fresh IMU at rest, and no hardware RTC writes. Official mode delegates every
// call unchanged.
class TestModePlatform : public Platform {
 public:
  TestModePlatform(Platform& real, adventure::TestModeHost& host);
  uint32_t now() override;
  uint32_t randomSeed() override;
  Sample sample() override;
  Input pollInput() override;
  device_clock::Reading readClock() override;
  device_clock::Reading writeClock(int64_t utc) override;
  void feedback(uint16_t duration) override;
  bool vibrationActive() override;
  bool usbHostPresent() override;
  void brightness(uint8_t value) override;
  c152::DisplayTransition displaySleep(bool sleeping) override;
  c152::BatteryStatus readBattery() override;

 private:
  void ensureSimulation();
  Platform& real_;
  adventure::TestModeHost& host_;
};

} }
