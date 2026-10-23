#include "board.hpp"

#include <stdio.h>

namespace {

uint32_t vibration_until_ms = 0;
bool vibration_active = false;
bool previous_touch_down = false;

}  // namespace

namespace c152 {

void begin(const char* title) {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.clear_display = true;
  config.internal_imu = true;
  config.internal_rtc = false;
  config.external_rtc = false;
  config.disable_rtc_irq = false;
  config.internal_spk = true;
  config.output_power = true;
  M5.begin(config);

  M5.Display.setRotation(0);
  M5.Display.setBrightness(96);
  M5.Display.setTextWrap(false);
  M5.Display.setSwapBytes(true);  // 素材数组保存原生 RGB565 数值。
  M5.Speaker.setVolume(72);
  previous_touch_down = M5.Touch.getDetail().isPressed();
  drawFooter(title, TFT_CYAN);

  Serial.printf("\nM5Stack StopWatch C152: %s\n", title);
  Serial.printf("display=%dx%d board=%d\n", M5.Display.width(),
                M5.Display.height(), static_cast<int>(M5.getBoard()));
}

void update() {
  M5.update();
  if (vibration_active &&
      static_cast<int32_t>(millis() - vibration_until_ms) >= 0) {
    M5.Power.setVibration(0);
    vibration_active = false;
  }
}

bool touchWasPressed() {
  const bool touch_down = M5.Touch.getDetail().isPressed();
  const bool pressed = touch_down && !previous_touch_down;
  previous_touch_down = touch_down;
  return pressed;
}

void feedback(uint16_t frequency_hz, uint16_t duration_ms,
              uint8_t vibration) {
  if (frequency_hz > 0) {
    M5.Speaker.tone(frequency_hz, duration_ms);
  }
  if (vibration > 0) {
    M5.Power.setVibration(vibration);
    vibration_until_ms = millis() + duration_ms;
    vibration_active = true;
  }
}

bool vibrationActive() { return vibration_active; }

void drawFooter(const char* text, uint16_t color) {
  M5.Display.setTextDatum(bottom_center);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(text, kScreenCenter, kScreenSize - 44);
}

}  // namespace c152
