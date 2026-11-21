#include "board_power.hpp"
#include <M5Unified.h>

namespace c152 {
namespace {
bool display_off = false;

bool sendPanelCommand(uint8_t cmd) {
  if (M5.getBoard() != m5::board_t::board_M5StopWatch) return false;
  auto* panel = M5.Display.panel();
  if (!panel || !panel->getBus() || panel->config().pin_cs < 0) return false;
  auto* bus = panel->getBus();
  const int cs = panel->config().pin_cs;
  bus->wait();
  bus->beginTransaction();
  bus->wait();
  lgfx::gpio_hi(cs);
  lgfx::gpio_lo(cs);
  bool accepted = true;
  const uint8_t command[] = {0x02, 0, cmd, 0};
  for (const uint8_t byte : command) accepted = bus->writeCommand(byte, 8) && accepted;
  bus->wait();
  lgfx::gpio_hi(cs);
  bus->endTransaction();
  return accepted;
}
}

BatteryStatus readBattery() {
  BatteryStatus status;
  const int level = M5.Power.getBatteryLevel();
  if (level >= 0 && level <= 100) status.percent = level;
  uint8_t gpio = 0;
  if (M5.Power.M5pm1.isEnabled() && M5.Power.M5pm1.readRegister(0x12, &gpio, 1))
    status.charge = gpio & (1 << 2) ? ChargeState::Discharging : ChargeState::Charging;
  return status;
}

void setDisplayBrightness(uint8_t brightness) {
  auto* panel = M5.Display.panel();
  // Framebuffer waitDMA is empty; drain the real bus before it is reconfigured.
  if (panel && panel->getBus()) panel->getBus()->wait();
  if (brightness > 0 && display_off && sendPanelCommand(0x29)) display_off = false;
  M5.Display.setBrightness(brightness);
}

DisplayTransition setDisplaySleep(bool sleeping) {
  // M5GFX 0.2.27's StopWatch framebuffer does not forward setSleep to the CO5300.
  // Use the physical driver's QSPI command format, without its blocking wake delay.
  if (M5.getBoard() != m5::board_t::board_M5StopWatch) return {false};
  if (sleeping) {
    const bool offOk = sendPanelCommand(0x28);
    if (offOk) display_off = true;
    const bool slpOk = offOk && sendPanelCommand(0x10);
    return {slpOk, 0u};
  }
  const bool ok = sendPanelCommand(0x11);
  return {ok, 150u};
}
}
