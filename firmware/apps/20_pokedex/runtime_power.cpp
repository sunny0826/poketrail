#include "runtime.hpp"
#include "viewer_screen.hpp"

namespace pokedex { namespace ui {
void Runtime::pollBattery(uint32_t now, bool force) {
  if (!force && (!safe() || now - lastBattery_ < 5000)) return;
  auto battery = platform_.readBattery();
  if (battery.percent < 0 || battery.percent > 100) battery.percent = -1;
  lastBattery_ = now;
  if (battery.percent != screen_.battery.percent || battery.charge != screen_.battery.charge) dirty_ = true;
  screen_.battery = battery;
  viewer_screen::setBattery(battery);
}

void Runtime::sleepDisplay() {
  if (display_ != DisplayState::Awake || !safe()) return;
  platform_.brightness(0);
  const auto result = platform_.displaySleep(true);
  if (result.accepted) display_ = DisplayState::Asleep;
  else {
    ++metrics_.displayErrors;
    platform_.brightness(96);
    lastActivity_ = platform_.now();
  }
  waitForRelease();
}

bool Runtime::updateDisplayPower(const Input& input) {
  if (display_ == DisplayState::Asleep && !releaseGate_ && (input.any() || input.held)) {
    const auto result = platform_.displaySleep(false);
    if (!result.accepted) { ++metrics_.displayErrors; waitForRelease(); return false; }
    wakeDelay_ = result.settleMs;
    wakeStarted_ = platform_.now();
    display_ = DisplayState::Waking;
    waitForRelease();
  }
  if (display_ != DisplayState::Waking) return false;
  waitForRelease();
  if (platform_.now() - wakeStarted_ < wakeDelay_) return false;
  display_ = DisplayState::Awake;
  lastActivity_ = platform_.now();
  pollBattery(lastActivity_, true);
  dirty_ = true;
  return true;
}
} }
