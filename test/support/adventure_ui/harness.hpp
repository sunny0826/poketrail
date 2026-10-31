#pragma once
#include <M5Unified.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "runtime.hpp"
#include "save_codec.hpp"
#include "encounter_catalog.hpp"
#include "board.hpp"
#include "../verification_output.hpp"
uint32_t host_time_ms = 0;
HostSerial Serial;
HostM5 M5;
namespace {
using namespace pokedex;
using namespace pokedex::adventure;
using namespace pokedex::ui;
unsigned checks = 0;
void check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
class MemoryStore : public Store {
 public:
  std::vector<uint8_t> bytes;
  LoadStatus status = LoadStatus::Empty;
  bool fail = false, writeThenFail = false;
  uint32_t delayMs = 0;
  unsigned commits = 0;
  LoadResult load(Save& s) override {
    if (status != LoadStatus::Loaded) return LoadResult(status);
    return LoadResult(codec::decode(bytes.data(), bytes.size(), s) == codec::DecodeStatus::Ok ? LoadStatus::Loaded : LoadStatus::Corrupt);
  }
  CommitStatus commit(const Save& s) override {
    ++commits; host_time_ms += delayMs;
    if (!fail || writeThenFail) {
      std::vector<uint8_t> encoded(codec::maxEncodedSize); size_t n = 0;
      check(codec::encode(s, encoded.data(), encoded.size(), n), "every UI commit is a legal encoded save");
      encoded.resize(n); bytes = encoded; status = LoadStatus::Loaded;
    }
    return fail ? CommitStatus::IoError : CommitStatus::Committed;
  }
};
class TestPlatform : public Platform {
 public:
  Input next;
  float z = 1;
  bool fresh = true, rtcValid = true, writeValid = true, imuError = false;
  uint8_t light = 96;
  uint32_t vibrationEnd = 0, rtcSetAt = 0, seed = 1;
  int64_t utc = 1792627200;
  unsigned clockWrites = 0, feedbacks = 0;
  MemoryStore* durable = nullptr;
  bool vibration = false;
  uint32_t now() override { return host_time_ms; }
  uint32_t randomSeed() override { return seed; }
  Sample sample() override {
    Sample s; s.fresh = fresh; s.ioError = imuError; s.z = z; s.at = now(); return s;
  }
  Input pollInput() override {
    c152::update();
    if (vibration && int32_t(now() - vibrationEnd) >= 0) vibration = false;
    Input r = next; r.at = now();
    r.leftDown = r.leftDown || r.left; r.rightDown = r.rightDown || r.right;
    r.touchDown = r.touchDown || r.touch;
    next = Input(); return r;
  }
  device_clock::Reading readClock() override {
    device_clock::Reading r = {}; r.valid = rtcValid; r.utcSeconds = utc + (now() - rtcSetAt) / 1000; return r;
  }
  device_clock::Reading writeClock(int64_t value) override {
    if (durable) {
      Save committed;
      check(durable->load(committed).status == LoadStatus::Loaded && committed.clock.pending.active &&
            committed.clock.pending.targetUtcSeconds == value, "RTC write follows durable pending transaction");
    }
    ++clockWrites; utc = value; rtcSetAt = now();
    auto r = readClock(); r.valid = writeValid; return r;
  }
  void feedback(uint16_t duration) override { ++feedbacks; vibration = true; vibrationEnd = now() + duration; }
  bool vibrationActive() override { return vibration; }
  void brightness(uint8_t value) override { light = value; }
  c152::DisplayTransition displaySleep(bool) override { return {true}; }
  c152::BatteryStatus readBattery() override { return {}; }
};
void frame(Runtime& r, uint32_t ms = 20) { host_time_ms += ms; r.loop(); }
void idle(Runtime& r, uint32_t duration) { for (uint32_t t = 0; t < duration; t += 20) frame(r); }
void edge(Runtime& r, TestPlatform& p, bool left, bool right) {
  p.next.left = left; p.next.right = right; p.next.held = true; frame(r); frame(r);
}
void tap(Runtime& r, TestPlatform& p, int x, int y) {
  p.next.touch = true; p.next.held = true; p.next.x = x; p.next.y = y; frame(r);
  p.next.click = true; p.next.back = y < 80; p.next.x = x; p.next.y = y; frame(r);
}
void openMenu(Runtime& r, TestPlatform& p) {
  check(r.screen().page == Page::Home, "open menu starts from exploration");
  frame(r); edge(r, p, false, true); frame(r);
  check(r.screen().page == Page::Menu && r.screen().selected == 0, "opening key only displays menu");
}
void select(Runtime& r, TestPlatform& p, unsigned item) {
  const auto page = r.screen().page;
  const unsigned count = menuCount(r.screen(), r.engine());
  check(item < count, "requested menu item exists");
  const bool paged = r.screen().page == Page::Menu || r.screen().page == Page::Clock ||
                      r.screen().page == Page::PartnerDetail || r.screen().page == Page::TestHub;
  const bool record = r.screen().page == Page::FirstCaptures || r.screen().page == Page::Recent ||
                      r.screen().page == Page::Days || r.screen().page == Page::Achievements;
  if (record) {
    for (unsigned guard = 0; r.screen().selected != item && guard < 2200; ++guard) {
      p.next.swipe = 1;
      frame(r);
    }
  }
  if (paged) {
    for (unsigned guard = 0; r.screen().selected / menu_layout::kPageSize != item / menu_layout::kPageSize &&
                             guard < 8; ++guard) {
      p.next.swipe = 1;
      frame(r);
    }
    frame(r);
  }
  if (!record && !(paged && r.screen().selected == item)) {
    for (unsigned guard = 0; r.screen().selected != item && guard < count; ++guard)
      edge(r, p, false, true);
  }
  if (r.screen().page != page || r.screen().selected != item)
    std::fprintf(stderr, "select page=%u actual=%u selected=%u wanted=%u count=%u\n", unsigned(page), unsigned(r.screen().page), r.screen().selected, item, menuCount(r.screen(), r.engine()));
  check(r.screen().page == page && r.screen().selected == item, "menu item reachable with physical keys");
  p.next.left = p.next.right = p.next.held = true;
  frame(r);
  p.next = Input();
  frame(r);
}
void screenshot(const char* name) {
  const std::string path = verificationOutput((std::string(name) + ".ppm").c_str());
  FILE* file = std::fopen(path.c_str(), "wb"); check(file != nullptr, "open PPM evidence");
  std::fprintf(file, "P6\n468 468\n255\n");
  bool inside = true;
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x) {
    uint16_t p = M5.Display.readPixel(x, y);
    if (p && (x-233)*(x-233)+(y-233)*(y-233) > 228*228) inside = false;
    unsigned char rgb[] = {static_cast<unsigned char>((p>>11)*255/31),
      static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};
    std::fwrite(rgb, 1, 3, file);
  }
  std::fclose(file); check(inside, "all published pixels inside safe round screen");
}
void snapCurrent(Runtime&, const char* name) { screenshot(name); }
void published(Runtime& r, const char* name) {
  M5.Display.beginObservation(); frame(r, 500); M5.Display.endObservation();
  const auto& frames = M5.Display.publishedFrames();
  check(frames.size() == 1 && frames[0] == M5.Display.framebuffer(), "new UI publishes exactly one complete frame");
  screenshot(name);
}
void walk(Runtime& r, TestPlatform& p, unsigned cycles) {
  for (unsigned i = 0; i < cycles; ++i) for (unsigned t = 0; t < 500; t += 20) {
    p.z = 1 - 0.30f * std::cos(6.28318530718f * t / 500); frame(r);
  }
  p.z = 1;
}
void start(Runtime& r) { check(r.begin(), "graphics allocated"); frame(r); }
void skipDateAndStarter(Runtime& r, TestPlatform& p) {
  check(r.screen().page == Page::DateWelcome, "first boot offers manual date or explicit skip");
  select(r, p, 1); check(r.screen().page == Page::Starter, "skip reaches starter");
  select(r, p, 0); check(r.screen().page == Page::Home, "starter reaches home");
}
void reachBattle(Runtime& r, TestPlatform& p) {
  if (r.dimmed()) edge(r, p, false, true);
  openMenu(r, p); select(r, p, 0); check(r.screen().page == Page::Encounter, "saved encounter opens instructions");
  select(r, p, 0); check(r.screen().page == Page::Battle, "explicit start enters battle");
  check(!r.engine().battleView().consumed, "start input cannot hit first cue");
}
void win(Runtime& r, TestPlatform& p) {
  for (unsigned i = 0; r.engine().view().phase == Phase::Battle && i < 15; ++i) {
    auto b = r.engine().battleView();
    while (int32_t(b.targetMs - host_time_ms) > 20) frame(r);
    p.next.left = b.expected == Action::Dodge; p.next.right = b.expected == Action::Attack;
    p.next.held = true; frame(r, b.targetMs - host_time_ms); frame(r);
    if (r.engine().view().phase == Phase::Battle) idle(r, 200);
  }
  check(r.engine().view().phase == Phase::ThrowReady, "public rhythm inputs win and persist victory");
  check(!r.engine().throwView().active, "winning input does not start throw");
}
// Keep the throw input held; the window may time out before release.
void holdThrow(Runtime& r, TestPlatform& p, unsigned ms) {
  p.next.right = p.next.held = true; frame(r);
  for (unsigned t = 0; t < ms; t += 20) {
    p.next.rightDown = p.next.held = true;
    frame(r);
  }
}
// Holds the throw key and releases it exactly on the ideal target, so the
// throw scores Perfect. The window opens on the first hold, so an already
// active window is reused instead of preparing a second ball.
void throwAt(Runtime& r, TestPlatform& p) {
  if (!r.engine().throwView().active) {
    p.next.right = p.next.held = true;
    frame(r);
  }
  const uint32_t target = r.engine().throwView().targetMs;
  while (int32_t(target - host_time_ms) > 20) { p.next.rightDown = p.next.held = true; frame(r); }
  p.next.rightDown = p.next.held = true;
  frame(r, target - host_time_ms);
  frame(r, 0);
}
}
