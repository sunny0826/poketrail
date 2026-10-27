#pragma once

#include "progression_ui.hpp"
#include "sprites_generated.hpp"

using namespace progression_ui;

namespace pokedex { namespace viewer_screen { extern viewer::State g_state; extern bool g_ready; } }
namespace {
class PowerPlatform : public HostPlatform {
 public:
  c152::BatteryStatus battery;
  unsigned reads = 0, sleeps = 0, wakes = 0, feedbacks = 0, clockWrites = 0;
  bool failSleep = false, failWake = false, observeWake = false, usb = false;
  std::vector<uint16_t> litFrame;
  c152::DisplayTransition displaySleep(bool sleeping) override {
    if (sleeping) { ++sleeps; return {!failSleep}; }
    ++wakes; return {!failWake, 150};
  }
  c152::BatteryStatus readBattery() override { ++reads; return battery; }
  bool usbHostPresent() override { return usb; }
  void feedback(uint16_t ms) override { ++feedbacks; HostPlatform::feedback(ms); }
  device_clock::Reading writeClock(int64_t) override { ++clockWrites; return {}; }
  void brightness(uint8_t value) override {
    if (observeWake && value) {
      check(M5.Display.brightness == 0, "wake redraw happens before restoring brightness");
      check(M5.Display.publishedFrames().size() == 1, "wake publishes exactly one complete frame before lighting");
      litFrame = M5.Display.framebuffer();
    }
    HostPlatform::brightness(value);
  }
};
void ready(MemoryStore& store, unsigned steps = 0) {
  Engine engine(store); check(engine.begin() == CommandResult::Applied, "power fixture boots");
  engine.chooseStarter(1); step(engine, 1, steps); engine.checkpoint();
}
}
