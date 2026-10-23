#pragma once
#include <cstdint>

namespace c152 {
enum class ChargeState : uint8_t { Unknown, Charging, Discharging };
struct BatteryStatus {
  int16_t percent = -1;
  ChargeState charge = ChargeState::Unknown;
};
BatteryStatus readBattery();
void setDisplayBrightness(uint8_t brightness);
struct DisplayTransition {
  bool accepted;
  uint32_t settleMs;
  DisplayTransition(bool ok, uint32_t ms = 0) : accepted(ok), settleMs(ms) {}
};
// The caller keeps sampling during the returned settling time.
DisplayTransition setDisplaySleep(bool sleeping);
}
