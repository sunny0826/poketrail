#include "device_clock_hw.hpp"
#include "calendar.hpp"
#include <M5Unified.h>

namespace pokedex { namespace device_clock {
namespace {
const uint8_t kFlags = 0x1D;
const uint8_t kVblf = 0x80;
const uint8_t kVlf = 0x02;
const uint8_t kTestStop = 0xC0;

bool status(m5::RTC_Base& base, uint8_t (&bytes)[2], Reading& result) {
  if (!base.readRegister(kFlags, bytes, sizeof bytes)) {
    result.ioError = true;
    return false;
  }
  result.lowVoltage = result.lowVoltage || (bytes[0] & kVblf);
  if ((bytes[0] & kVlf) || (bytes[1] & kTestStop)) {
    result.verificationFailed = true;
    return false;
  }
  return true;
}
bool bcd(uint8_t raw, uint8_t mask, int& value) {
  // These reserved bits read as zero, not as alternate date/control fields.
  if ((raw & ~mask) || (raw & 15) > 9 || (raw >> 4) > 9) return false;
  value = (raw >> 4) * 10 + (raw & 15);
  return true;
}
bool decode(const uint8_t (&raw)[7], int64_t& utc) {
  calendar::CivilTime value = {};
  if (!bcd(raw[0], 0x7F, value.second)
      || !bcd(raw[1], 0x7F, value.minute)
      || !bcd(raw[2], 0x3F, value.hour)
      || !bcd(raw[4], 0x3F, value.day)
      || !bcd(raw[5], 0x1F, value.month)
      || !bcd(raw[6], 0xFF, value.year)
      || !raw[3] || (raw[3] & 0x80) || (raw[3] & (raw[3] - 1))) return false;
  value.year += 2000;
  return calendar::toUtc(value, 0, utc);
}

bool recover(m5::RTC_Base& base, Reading& result) {
  // Clock-only initialization: unused alarm/timer registers may remain RAM
  // (Epson ETM50E-10 pp57-58). Keep the board's charging/threshold settings.
  uint8_t power = 0, extension = 0;
  if (!base.readRegister(0x1F, &power, 1)
      || !base.readRegister(0x31, &extension, 1)
      || !base.writeRegister8(0x1E, 0)  // TEST, STOP and IRQ enables off.
      || !base.writeRegister8(0x1C, 0x04) // TE off, reset extension settings.
      || !base.writeRegister8(0x30, 0) // No digital correction.
      || !base.writeRegister8(0x31, extension & 1) // Reserved TEST bits off.
      || !base.writeRegister8(0x1F, (power & 0xF7) | 0x10)) {
    result.ioError = true;
    return false;
  }
  return true;
}
}

namespace {
bool writeVerified(m5::RTC_Base& base, uint8_t reg, uint8_t value,
                   Reading& result, uint8_t mask = 0xFF) {
  uint8_t actual = 0;
  if (!base.writeRegister8(reg, value) || !base.readRegister(reg, &actual, 1)) {
    result.ioError = true;
    return false;
  }
  if ((actual & mask) != (value & mask)) {
    result.verificationFailed = true;
    return false;
  }
  return true;
}
}

void Clock::begin(m5::I2C_Class& bus) {
  if (phase_ != Phase::Cold) return;
  bus_ = &bus;
  rx_.attach(bus);
  initialize();
}

Reading Clock::phaseReading() const {
  Reading result = {};
  switch (phase_) {
    case Phase::Cold: case Phase::NoBus: result.unavailable = true; break;
    case Phase::InitIoError: result.ioError = true; break;
    case Phase::InitVerificationFailed: result.verificationFailed = true; break;
    case Phase::Ready: break;
  }
  return result;
}

bool Clock::markDirty(Reading& failure) {
  calibrationRequired_ = true;
  return writeVerified(rx_.transport(), 0x20, 0xA5, failure);
}

void Clock::restoreDirty() {
  // A clean write may have reached the RTC even when its ACK/readback failed.
  // One best-effort repair only; it can never turn a failed transaction valid.
  rx_.transport().writeRegister8(0x20, 0xA5);
}

void Clock::initialize() {
  rx_.enableDateWrites(false);
  if (!bus_ || !bus_->isEnabled()) { phase_ = Phase::NoBus; return; }
  auto& base = rx_.transport();
  uint8_t initial[2] = {}, power = 0, extension = 0, marker = 0;
  phase_ = Phase::InitIoError;
  if (!base.readRegister(kFlags, initial, sizeof initial)
      || !base.readRegister(0x1F, &power, 1)
      || !base.readRegister(0x31, &extension, 1)
      || !base.readRegister(0x20, &marker, 1)) return;
  const bool hardwareFault = (initial[1] & kTestStop) || (extension & 0xFE);
  calibrationRequired_ = calibrationRequired_ || hardwareFault || marker == 0xA5;
  Reading failure = {};
  if ((hardwareFault && marker != 0xA5 && !markDirty(failure))
      || !writeVerified(base, 0x1E, 0, failure, 0xF8)
      || ((extension & 0xFE) && !writeVerified(base, 0x31, extension & 1, failure))
      || !writeVerified(base, 0x1F, (power & 0xF7) | 0x30, failure)
      || !writeVerified(base, 0x30, 0, failure)) {
    phase_ = failure.ioError ? Phase::InitIoError : Phase::InitVerificationFailed;
    return;
  }
  phase_ = Phase::Ready;
  rx_.enableDateWrites(true);
}

Reading Clock::sampleRaw() {
  Reading result = {};
  auto& base = rx_.transport();
  uint8_t state[2] = {};
  if (!status(base, state, result)) return result;
  uint8_t raw[7] = {}, extension = 0;
  if (!base.readRegister(0x10, raw, sizeof raw)) {
    result.ioError = true;
    return result;
  }
  // Decode only this burst, never the driver's unchecked ctz/BCD getter.
  const bool decoded = decode(raw, result.utcSeconds);
  result.invalidDate = !decoded;
  if (!base.readRegister(0x31, &extension, 1)) {
    result.ioError = true;
    return result;
  }
  if (extension & 0xFE) result.verificationFailed = true;
  // Keep Flag last so faults raised during the extension read are visible too.
  if (!status(base, state, result)) return result;
  result.valid = decoded && !result.lowVoltage && !result.verificationFailed;
  return result;
}

Reading Clock::read() {
  if (phase_ != Phase::Ready) return phaseReading();
  Reading result = sampleRaw();
  if (calibrationRequired_) {
    result.valid = false;
    result.verificationFailed = true;
  }
  return result;
}

Reading Clock::verifyWindow(int64_t utcSeconds) {
  Reading result = sampleRaw();
  if (result.valid && (result.utcSeconds < utcSeconds || result.utcSeconds - utcSeconds > 2)) {
    result.valid = false;
    result.verificationFailed = true;
  }
  return result;
}

Reading Clock::setAndVerify(int64_t utcSeconds) {
  Reading result = {};
  calendar::CivilTime value;
  if (!calendar::fromUtc(utcSeconds, 0, value)) {
    result.invalidDate = true;
    return result;
  }
  if (phase_ == Phase::InitIoError || phase_ == Phase::InitVerificationFailed
      || (phase_ == Phase::NoBus && bus_ && bus_->isEnabled())) initialize();
  if (phase_ != Phase::Ready) return phaseReading();
  auto& base = rx_.transport();
  uint8_t initial[2] = {}, extension = 0;
  if (!base.readRegister(kFlags, initial, sizeof initial)
      || !base.readRegister(0x31, &extension, 1)) {
    result.ioError = true;
    return result;
  }
  const bool hadFault = calibrationRequired_ || (initial[0] & kVlf)
      || (initial[1] & kTestStop) || (extension & 0xFE);
  if (!markDirty(result)) return result;
  if (hadFault) {
    m5::M5Unified::delay(40);
    if (!recover(base, result)) return result;
  }
  const uint8_t clear = initial[0] & (kVblf | kVlf);
  if (clear && !base.writeRegister8(kFlags, uint8_t(0xBE & ~clear))) {
    result.ioError = true;
    return result;
  }
  uint8_t state[2] = {};
  if (!status(base, state, result) || result.lowVoltage) return result;
  uint8_t weekday = 0;
  calendar::weekday(value, weekday);
  const m5::rtc_date_t date(value.year, value.month, value.day, weekday);
  const m5::rtc_time_t time(value.hour, value.minute, value.second);
  if (!base.setDateTime(&date, &time)) {
    result.ioError = true;
    return result;
  }
  // No Flag write after the date write, including on any failure path.
  result = verifyWindow(utcSeconds);
  if (!result.valid) return result;
  result.valid = false;
  if (!writeVerified(base, 0x20, 0, result)) {
    restoreDirty();
    return result;
  }
  result = verifyWindow(utcSeconds);
  if (!result.valid) {
    restoreDirty();
    return result;
  }
  calibrationRequired_ = false;
  return result;
}
} }
