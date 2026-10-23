#pragma once
#include "device_clock.hpp"
#include <utility/rtc/RX8130_Class.hpp>

namespace pokedex { namespace device_clock {
class Clock final {
 public:
  Clock() = default;
  Clock(const Clock&) = delete;
  Clock& operator=(const Clock&) = delete;
  Clock(Clock&&) = delete;
  Clock& operator=(Clock&&) = delete;
  void begin(m5::I2C_Class& bus);
  Reading read();
  Reading setAndVerify(int64_t utcSeconds);

 private:
  class AttachedRx final : private m5::RX8130_Class {
   public:
    AttachedRx() : RX8130_Class(0x32, 400000, nullptr) {}
    void attach(m5::I2C_Class& bus) { _i2c = &bus; _init = false; }
    void enableDateWrites(bool enabled) { _init = enabled; }
    m5::RTC_Base& transport() { return *this; }
  } rx_;
  enum class Phase : uint8_t { Cold, Ready, NoBus, InitIoError, InitVerificationFailed };
  Phase phase_ = Phase::Cold;
  m5::I2C_Class* bus_ = nullptr;
  bool calibrationRequired_ = false;
  void initialize();
  Reading phaseReading() const;
  Reading sampleRaw();
  Reading verifyWindow(int64_t utcSeconds);
  bool markDirty(Reading& failure);
  void restoreDirty();
};
} }
