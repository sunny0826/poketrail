#include "board.hpp"

#include <stdio.h>
#if defined(ESP32) || defined(ESP32S3)
#include <driver/gpio.h>
#include <esp_sleep.h>
#endif

namespace {

uint32_t vibration_until_ms = 0;
bool vibration_active = false;
bool previous_touch_down = false;

// L1 同时供电 RTC 与 IMU；hold 使双击关机后 RTC 继续走时。
constexpr uint8_t kPm1HoldCfg = 0x07;
constexpr uint8_t kPm1LdoHold = 0x20;

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

  uint8_t hold = 0;
  if (!M5.Power.M5pm1.readRegister(kPm1HoldCfg, &hold, 1)) {
    Serial.printf("pm1 ldo hold failed: read\n");
  } else if (!M5.Power.M5pm1.writeRegister8(kPm1HoldCfg,
                                             hold | kPm1LdoHold)) {
    Serial.printf("pm1 ldo hold failed: write\n");
  } else if (!M5.Power.M5pm1.readRegister(kPm1HoldCfg, &hold, 1)) {
    Serial.printf("pm1 ldo hold failed: verify\n");
  } else if (!(hold & kPm1LdoHold)) {
    Serial.printf("pm1 ldo hold failed: verify %02x\n", hold);
  } else {
    Serial.printf("pm1 ldo hold=%02x\n", hold);
  }
}

void update() {
  M5.update();
  if (vibration_active &&
      static_cast<int32_t>(millis() - vibration_until_ms) >= 0) {
    M5.Power.setVibration(0);
    vibration_active = false;
  }
}

bool restUntilInput(uint32_t durationMs) {
#if !defined(ESP32) && !defined(ESP32S3)
  (void)durationMs;
  return false;
#else
  if (durationMs == 0) return true;
  // StopWatch keys are active-low. CST816S INT on GPIO13 is active-low while touched.
  const gpio_num_t pins[] = {GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_13};
  bool armed = true;
  for (const gpio_num_t pin : pins) {
    if (gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL) != ESP_OK) armed = false;
  }
  if (!armed || esp_sleep_enable_gpio_wakeup() != ESP_OK ||
      esp_sleep_enable_timer_wakeup(uint64_t(durationMs) * 1000) != ESP_OK) {
    for (const gpio_num_t pin : pins) gpio_wakeup_disable(pin);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    return false;
  }
  const esp_err_t slept = esp_light_sleep_start();
  for (const gpio_num_t pin : pins) gpio_wakeup_disable(pin);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  return slept == ESP_OK;
#endif
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
