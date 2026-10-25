#include <Arduino.h>
#include <M5Unified.h>
#include <esp_system.h>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include "board.hpp"
#include "runtime.hpp"
#include "button_input.hpp"
#include "device_store.hpp"
#include "device_clock_hw.hpp"

namespace {
class DevicePlatform : public pokedex::ui::Platform {
 public:
  void beginClock() { clock_.begin(M5.In_I2C); }
  uint32_t now() override { return millis(); }
  uint32_t randomSeed() override { return esp_random(); }
  pokedex::ui::Sample sample() override {
    pokedex::ui::Sample s;
    const auto mask = M5.Imu.update();
    s.at = millis();
    s.fresh = (mask & m5::IMU_Class::sensor_mask_accel) != 0;
    // The installed update API does not distinguish I/O failure from no new data.
    if (s.fresh) {
      const auto& data = M5.Imu.getImuData();
      s.x = data.accel.x; s.y = data.accel.y; s.z = data.accel.z;
    }
    return s;
  }
  pokedex::ui::Input pollInput() override {
    c152::update();
    pokedex::ui::Input input;
    input.at = millis();
    buttons_.sample(input, M5.BtnA, M5.BtnB);
    input.touch = c152::touchWasPressed();
    const auto detail = M5.Touch.getDetail();
    input.x = detail.x; input.y = detail.y;
    const int dx = detail.x - detail.base_x, dy = detail.y - detail.base_y;
    input.click = detail.wasReleased() && detail.wasClicked() && std::abs(dx) <= 12 && std::abs(dy) <= 12;
    input.back = input.click && detail.base_y < 80 && detail.y < 80;
    if (detail.wasReleased() && std::abs(dx) >= 45 && std::abs(dx) > std::abs(dy))
      input.swipe = dx < 0 ? 1 : -1;
    input.held = input.held || detail.isPressed();
    lastInput_ = input;
    return input;
  }
  const pokedex::ui::Input& lastInput() const { return lastInput_; }
  pokedex::device_clock::Reading readClock() override { return clock_.read(); }
  pokedex::device_clock::Reading writeClock(int64_t utc) override { return clock_.setAndVerify(utc); }
  void feedback(uint16_t duration) override { c152::feedback(0, duration, 70); }
  bool vibrationActive() override { return c152::vibrationActive(); }
  void brightness(uint8_t value) override { c152::setDisplayBrightness(value); }
  c152::DisplayTransition displaySleep(bool sleeping) override { return c152::setDisplaySleep(sleeping); }
  c152::BatteryStatus readBattery() override { return c152::readBattery(); }
 private:
  pokedex::device_clock::Clock clock_;
  pokedex::ui::Input lastInput_;
  pokedex::ui::ButtonInput buttons_;
};
class MountedStore : public pokedex::adventure::Store {
 public:
  void begin() { device_.begin(); }
  pokedex::adventure::LoadResult load(pokedex::adventure::Save& out) override {
    const auto status = device_.begin();
    if (status != pokedex::adventure::LoadStatus::Loaded) return pokedex::adventure::LoadResult(status);
    return device_.load(out);
  }
  pokedex::adventure::CommitStatus commit(const pokedex::adventure::Save& save) override {
    return device_.commit(save);
  }
 private:
  pokedex::adventure::DeviceStore device_;
};
MountedStore store;
DevicePlatform platform;
pokedex::ui::Runtime runtime(store, platform);
bool ready = false;
void diagnostic(const char* format, ...) {
  char line[256];
  va_list args; va_start(args, format);
  const int length = vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  if (length > 0 && length < int(sizeof(line)) && Serial.availableForWrite() >= length)
    Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(length));
}
}
void setup() {
  c152::begin("ADVENTURE");
  platform.beginClock();
  store.begin();
  ready = runtime.begin();
  if (!ready) {
    pokedex::ui::drawGraphicsError(runtime.screen().battery);
  }
}
void loop() {
  const auto beforePage = runtime.screen().page;
  const auto beforeSelected = runtime.screen().selected;
  runtime.loop();
  static bool leftDown = false, rightDown = false;
  const auto& input = platform.lastInput();
  if (input.any() || input.leftDown != leftDown || input.rightDown != rightDown) {
    diagnostic("ui t=%lu touch=%u xy=%d,%d keys=%u,%u down=%u,%u held=%u page=%u>%u row=%u>%u dim=%u ready=%u pending=%u\n",
      (unsigned long)input.at, unsigned(input.touch), int(input.x), int(input.y),
      unsigned(input.left), unsigned(input.right), unsigned(input.leftDown), unsigned(input.rightDown), unsigned(input.held),
      unsigned(beforePage), unsigned(runtime.screen().page), unsigned(beforeSelected),
      unsigned(runtime.screen().selected), unsigned(runtime.dimmed()),
      unsigned(runtime.engine().ready()), unsigned(runtime.engine().savePending()));
  }
  leftDown = input.leftDown; rightDown = input.rightDown;
  static uint32_t report = 0;
  if (millis() - report >= 10000) {
    report = millis(); const auto& m = runtime.metrics();
    diagnostic("sample=%lu maxgap=%lu input=%lu gaps=%lu noaccel=%lu imuio=not-exposed rtc=%lu render=%lu commit=%lu\n",
      (unsigned long)m.samples, (unsigned long)m.maxSampleGapMs, (unsigned long)m.maxInputGapMs,
      (unsigned long)m.qualityGaps, (unsigned long)m.noSample,
      (unsigned long)m.rtcErrors, (unsigned long)m.maxRenderMs, (unsigned long)m.maxCommitMs);
    const auto& save = runtime.engine().view();
    diagnostic("state page=%u row=%u ready=%u pending=%u starter=%u partner=%u phase=%u seq=%llu total=%llu balls=%llu\n",
      unsigned(runtime.screen().page), unsigned(runtime.screen().selected), unsigned(runtime.engine().ready()),
      unsigned(runtime.engine().savePending()), unsigned(save.starter), unsigned(save.partner), unsigned(save.phase),
      (unsigned long long)save.sequence, (unsigned long long)save.totalSteps,
      (unsigned long long)pokedex::adventure::availableBalls(save));
    const auto& battery = runtime.screen().battery;
    diagnostic("power dim=%u battery=%d charge=%u displayerr=%lu\n",
      unsigned(runtime.dimmed()), int(battery.percent), unsigned(battery.charge), (unsigned long)m.displayErrors);
  }
  const uint32_t wait = runtime.nextSampleMs(millis());
  if (!(runtime.pocketIdle() && wait > 0 && c152::restUntilInput(wait))) delay(1);
}
