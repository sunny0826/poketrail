#pragma once

#include "progression_ui.hpp"

using namespace progression_ui;

namespace pokedex { namespace viewer_screen { extern bool g_ready; } }
namespace {
class PowerPlatform : public HostPlatform {
 public:
  unsigned clockWrites = 0;
  bool failSleep = false, failWake = false;
  c152::DisplayTransition displaySleep(bool sleeping) override {
    if (sleeping) return {!failSleep};
    return {!failWake, 150};
  }
  device_clock::Reading writeClock(int64_t) override { ++clockWrites; return {}; }
};
void ready(MemoryStore& store, unsigned steps = 0) {
  Engine engine(store); check(engine.begin() == CommandResult::Applied, "power fixture boots");
  engine.chooseStarter(1); step(engine, 1, steps); engine.checkpoint();
}
}
