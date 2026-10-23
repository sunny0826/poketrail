#include "battery_screen.hpp"
#include <M5Unified.h>
#include <cstdio>

namespace pokedex { namespace ui {
void drawBattery(const c152::BatteryStatus& status) {
  const bool known = status.percent >= 0 && status.percent <= 100;
  const uint16_t color = known && status.percent <= 20 ? TFT_RED : TFT_WHITE;
  M5.Display.fillRect(192, 8, 82, 17, TFT_BLACK);
  M5.Display.drawRoundRect(198, 11, 23, 12, 2, color);
  M5.Display.fillRect(221, 14, 2, 6, color);
  if (known) M5.Display.fillRect(201, 14, status.percent * 17 / 100, 6, color);
  if (status.charge == c152::ChargeState::Charging) {
    M5.Display.fillTriangle(211, 10, 204, 18, 210, 18, TFT_YELLOW);
    M5.Display.fillTriangle(209, 16, 215, 16, 208, 24, TFT_YELLOW);
  }
  char value[8];
  if (known) snprintf(value, sizeof(value), "%d%%", int(status.percent));
  else snprintf(value, sizeof(value), "--");
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(value, 228, 17);
}
} }
