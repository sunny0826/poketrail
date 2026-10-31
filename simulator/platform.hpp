#pragma once
#include <SDL.h>
#include <M5Unified.h>
#include "board.hpp"
#include <array>
#include <random>
#include "button_input.hpp"
#include "storage.hpp"

namespace simulator {
constexpr int kWidth = 568, kHeight = 640, kScreenX = 50, kScreenY = 24, kScreenSize = 468;
class Platform : public pokedex::ui::Platform {
 public:
  explicit Platform(Storage& storage, uint32_t seed);
  void event(const SDL_Event& event);
  void finishFrame();
  bool walking() const { return walking_; }
  bool fast() const { return fast_; }
  bool takeScreenshot();
  uint32_t now() override { return host_time_ms; }
  uint32_t randomSeed() override { return random_(); }
  pokedex::ui::Sample sample() override;
  pokedex::ui::Input pollInput() override;
  pokedex::device_clock::Reading readClock() override;
  pokedex::device_clock::Reading writeClock(int64_t utc) override;
  bool saveClock() { return storage_.saveClock(readClock().utcSeconds); }
  void feedback(uint16_t ms) override { vibrationEnd_ = now() + ms; }
  bool vibrationActive() override { return int32_t(vibrationEnd_ - now()) > 0; }
  void brightness(uint8_t value) override { M5.Display.setBrightness(value); }
  c152::DisplayTransition displaySleep(bool) override { return {true}; }
  c152::BatteryStatus readBattery() override { return {}; }
 private:
  void release();
  Storage& storage_;
  pokedex::ui::ButtonInput buttons_;
  std::mt19937 random_;
  std::array<bool, SDL_NUM_SCANCODES> keys_{};
  int64_t clockUtc_;
  uint32_t clockAt_ = 0, vibrationEnd_ = 0, walkAt_ = 0;
  bool walking_ = false, fast_ = false, power_ = false, screenshot_ = false;
};
}
