#pragma once
#include <M5Unified.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "runtime.hpp"
#include "button_input.hpp"
#include "save_codec.hpp"
#include "encounter_catalog.hpp"
#include "viewer_screen.hpp"
#include "board.hpp"
#include "verification_output.hpp"
#include "rendered_text.hpp"

uint32_t host_time_ms = 0;
HostSerial Serial;
HostM5 M5;
namespace progression_ui {
using namespace pokedex;
using namespace pokedex::adventure;
using namespace pokedex::ui;
unsigned checks = 0;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
class MemoryStore : public Store {
 public:
  std::vector<uint8_t> bytes;
  bool fail = false, writeThenFail = false;
  unsigned commits = 0;
  LoadResult load(Save& save) override {
    if (bytes.empty()) return LoadResult(LoadStatus::Empty);
    return LoadResult(codec::decode(bytes.data(), bytes.size(), save) == codec::DecodeStatus::Ok ?
                      LoadStatus::Loaded : LoadStatus::Corrupt);
  }
  CommitStatus commit(const Save& save) override {
    ++commits;
    if (!fail || writeThenFail) {
      std::vector<uint8_t> encoded(codec::maxEncodedSize); size_t length = 0;
      check(codec::encode(save, encoded.data(), encoded.size(), length), "UI save uses production encoder and validator");
      encoded.resize(length); bytes = encoded;
    }
    return fail ? CommitStatus::IoError : CommitStatus::Committed;
  }
};
class HostPlatform : public Platform {
 public:
  float z = 1;
  uint32_t vibrationEnd = host_time_ms;
  ButtonInput buttonInput;
  uint32_t now() override { return host_time_ms; }
  uint32_t randomSeed() override { return 7; }
  Sample sample() override { Sample s; s.fresh = true; s.z = z; s.at = now(); return s; }
  Input pollInput() override {
    c152::update(); Input input;
    input.at = now();
    uint8_t button = 0;
    if (M5.Power.M5pm1.readRegister(0x48, &button, 1)) {
      const bool down = (button & 0x01) != 0;
      input.power = (down && !powerWasDown_) || ((button & 0x80) != 0 && !down);
      powerWasDown_ = down;
    }
    buttonInput.sample(input, M5.BtnA, M5.BtnB);
    input.touch = c152::touchWasPressed(); const auto detail = M5.Touch.getDetail();
    input.x = detail.x; input.y = detail.y;
    const int dx = detail.x - detail.base_x, dy = detail.y - detail.base_y;
    input.click = detail.wasReleased() && detail.wasClicked() && std::abs(dx) <= 12 && std::abs(dy) <= 12;
    input.back = input.click && detail.base_y < 80 && detail.y < 80;
    if (detail.wasReleased() && std::abs(dx) >= 45 && std::abs(dx) > std::abs(dy))
      input.swipe = dx < 0 ? 1 : -1;
    input.touchDown = detail.isPressed();
    input.held = input.held || input.touchDown;
    return input;
  }
  device_clock::Reading readClock() override { return {}; }
  device_clock::Reading writeClock(int64_t) override { return {}; }
  unsigned feedbacks = 0;
  uint16_t lastFeedback = 0;
  void feedback(uint16_t ms) override { ++feedbacks; lastFeedback = ms; vibrationEnd = now() + ms; }
  bool vibrationActive() override { return int32_t(vibrationEnd - now()) > 0; }
  void brightness(uint8_t value) override { M5.Display.setBrightness(value); }
  c152::DisplayTransition displaySleep(bool) override { return {true}; }
  c152::BatteryStatus readBattery() override { return {}; }
  bool powerWasDown_ = false;
};
void frame(Runtime& runtime, uint32_t ms = 20) { host_time_ms += ms; runtime.loop(); }
void idle(Runtime& runtime, unsigned ms) { for (unsigned i = 0; i < ms; i += 20) frame(runtime); }
void buttons(bool left, bool right) { M5.button_a_down = left; M5.button_b_down = right; }
void edge(Runtime& runtime, bool left, bool right) {
  buttons(left, right); frame(runtime);
  buttons(false, false); frame(runtime);
}
void tap(Runtime& runtime, int x, int y) {
  auto& detail = M5.Touch.detail;
  detail.base_x = detail.x = x; detail.base_y = detail.y = y; detail.pressed = true;
  frame(runtime); detail.pressed = false; detail.released = detail.clicked = true;
  frame(runtime); detail = HostTouchDetail();
}
void swipe(Runtime& runtime, int startX, int startY, int endX, int endY) {
  auto& detail = M5.Touch.detail;
  detail.base_x = detail.x = startX; detail.base_y = detail.y = startY; detail.pressed = true;
  frame(runtime); detail.x = endX; detail.y = endY;
  detail.pressed = false; detail.released = true; detail.clicked = false;
  frame(runtime); detail = HostTouchDetail();
}
void openMenu(Runtime& runtime, bool left = false) {
  check(runtime.screen().page == Page::Home, "menu entry starts on exploration display");
  edge(runtime, left, !left);
  const unsigned focus = encounterWaiting(runtime.engine().view()) ? 0 : unsigned(MenuItem::Region);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == focus,
        "fresh side key opens menu on its first selectable entry without confirming");
}
void select(Runtime& runtime, MenuItem item);
void select(Runtime& runtime, unsigned index) {
  Page page = runtime.screen().page;
  unsigned count = menuCount(runtime.screen(), runtime.engine());
  const bool record = page == Page::FirstCaptures || page == Page::Recent ||
                      page == Page::Days || page == Page::Achievements;
  if (!record && !hubFor(page) && count > menu_layout::kPageSize && index >= menu_layout::kPageSize) {
    while (runtime.screen().selected / menu_layout::kPageSize != index / menu_layout::kPageSize)
      swipe(runtime, 300, 300, 160, 300);
    frame(runtime);
  }
  for (unsigned i = 0; runtime.screen().selected != index && i < count + 1; ++i) edge(runtime, false, true);
  check(runtime.screen().selected == index, "menu reachable with real Button_Class edges");
  edge(runtime, true, true);
}
void select(Runtime& runtime, MenuItem item) {
  check(runtime.screen().page == Page::Menu, "named menu entry is chosen from the main menu");
  select(runtime, unsigned(item));
}
void start(Runtime& runtime) {
  M5.BtnA = m5::Button_Class(); M5.BtnB = m5::Button_Class(); buttons(false, false);
  M5.powerDown(false);
  M5.Touch.detail = HostTouchDetail();
  check(runtime.begin(), "runtime graphics ready"); frame(runtime);
}
void walk(Runtime& runtime, HostPlatform& platform, unsigned cycles) {
  for (unsigned c = 0; c < cycles; ++c) for (unsigned t = 0; t < 500; t += 20) {
    platform.z = 1 - 0.30f * std::cos(6.28318530718f * t / 500); frame(runtime);
  }
  platform.z = 1;
}
void step(Engine& engine, uint16_t partner, unsigned count) {
  for (unsigned i = 0; i < count; ++i) {
    StepEvent event; event.batchId = engine.view().lastStepBatch + 1; event.partner = partner;
    event.clock.bootGeneration = engine.view().bootGeneration;
    event.clock.monotonicMs = uint32_t(event.batchId * 20);
    auto result = engine.observeSteps(&event, 1, 7);
    check(result == CommandResult::Applied, "fixture uses public step events");
  }
}
void legacyOwned(MemoryStore& store, const std::vector<uint16_t>& ids, uint16_t partner = 1) {
  Save save; save.schema = 1; save.route = Route::Forest; save.bootGeneration = 1; save.starter = 1; save.partner = partner;
  save.encounter.ruleVersion = 1;
  for (auto& attempt : save.encounter.attempts) attempt.ruleVersion = 1;
  setOwned(save, 1);
  for (uint16_t id : ids) {
    if (id == 1) continue;
    setOwned(save, id); save.captureCounts[id - 1] = 1;
    Capture capture; capture.species = id; capture.at.bootGeneration = 1;
    capture.at.totalSteps = save.totalSteps += 500;
    save.firstCapture[id - 1] = capture; ++save.nextEncounterId;
  }
  save.undatedSteps = save.totalSteps;
  const uint64_t thresholds[] = {10000, 50000, 100000};
  for (unsigned i = 0; i < 3; ++i) if (save.totalSteps >= thresholds[i]) {
    save.lifetime[i].reached = true; save.lifetime[i].first.bootGeneration = 1;
    save.lifetime[i].first.totalSteps = thresholds[i];
  }
  check(validate(save), "legacy collection fixture validates before migration");
  check(store.commit(save) == CommitStatus::Committed, "legacy collection has authentic versioned bytes");
}
void screenshot(Runtime& runtime, const char* name, uint32_t advanceMs = 500) {
  frame(runtime, advanceMs);
  const std::string path = verificationOutput((std::string(name) + ".ppm").c_str());
  FILE* file = std::fopen(path.c_str(), "wb"); check(file, "open screenshot");
  std::fprintf(file, "P6\n468 468\n255\n");
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x) {
    uint16_t color = M5.Display.readPixel(x, y);
    uint8_t pixel[] = {uint8_t((color >> 11) * 255 / 31), uint8_t(((color >> 5) & 63) * 255 / 63), uint8_t((color & 31) * 255 / 31)};
    std::fwrite(pixel, 1, 3, file);
  }
  std::fclose(file);
}
}
