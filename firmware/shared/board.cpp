#include "board.hpp"

#include <stdio.h>
#if defined(ESP32) || defined(ESP32S3)
#include <esp_sleep.h>
#endif

namespace {

uint32_t vibration_until_ms = 0;
bool vibration_active = false;
bool previous_touch_down = false;

// L1 同时供电 RTC 与 IMU；hold 使双击关机后 RTC 继续走时。
constexpr uint8_t kPm1HoldCfg = 0x07;
constexpr uint8_t kPm1LdoHold = 0x20;
constexpr uint8_t kPm1PwrCfg = 0x06;
constexpr uint8_t kPm1LedEn = 0x10;
// PM1 单击复位使能时会直接复位主机，须禁用后由应用处理熄屏／唤醒。
constexpr uint8_t kPm1ButtonCfg1 = 0x49;
constexpr uint8_t kPm1SingleResetDisable = 0x01;

#if defined(ESP32) || defined(ESP32S3)
uint8_t bmi270_addr = 0;

void configureLowPowerImu() {
  constexpr uint32_t kImuI2cFreq = 400000;
  for (const uint8_t addr : {uint8_t(0x68), uint8_t(0x69)}) {
    uint8_t chipId = 0;
    if (!M5.In_I2C.readRegister(addr, 0x00, &chipId, 1, kImuI2cFreq) ||
        chipId != 0x24) {
      continue;
    }
    bool ok = M5.In_I2C.writeRegister8(addr, 0x7C, 0x00, kImuI2cFreq);
    delayMicroseconds(500);
    ok = ok && M5.In_I2C.writeRegister8(addr, 0x6B, 0x00, kImuI2cFreq);
    ok = ok && M5.In_I2C.writeRegister8(addr, 0x40, 0x27, kImuI2cFreq);
    ok = ok && M5.In_I2C.writeRegister8(addr, 0x41, 0x02, kImuI2cFreq);
    ok = ok && M5.In_I2C.writeRegister8(addr, 0x7D, 0x04, kImuI2cFreq);
    delayMicroseconds(500);
    uint8_t err = 0xff, acc = 0, pwr = 0;
    ok = ok && M5.In_I2C.readRegister(addr, 0x02, &err, 1, kImuI2cFreq) &&
         M5.In_I2C.readRegister(addr, 0x40, &acc, 1, kImuI2cFreq) &&
         M5.In_I2C.readRegister(addr, 0x7D, &pwr, 1, kImuI2cFreq);
    if (ok && err == 0 && acc == 0x27 && pwr == 0x04 &&
        M5.In_I2C.writeRegister8(addr, 0x7C, 0x01, kImuI2cFreq)) {
      bmi270_addr = addr;
      Serial.printf("bmi270 lp addr=%02x acc=%02x pwr=%02x\n", addr, acc, pwr);
    } else {
      bmi270_addr = 0;
      Serial.printf("bmi270 lp config failed: addr=%02x err=%02x acc=%02x pwr=%02x\n",
                    addr, err, acc, pwr);
    }
    return;
  }
}
#endif

}

namespace c152 {

void begin(const char* title) {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.clear_display = true;
  config.internal_imu = true;
  config.internal_rtc = false;
  config.external_rtc = false;
  config.disable_rtc_irq = false;
  config.internal_spk = false;
  config.internal_mic = false;
  config.pmic_button = false;
  config.output_power = false;
  M5.begin(config);

  M5.Display.setRotation(0);
  M5.Display.setBrightness(96);
  M5.Display.setTextWrap(false);
  M5.Display.setSwapBytes(true);
  previous_touch_down = M5.Touch.getDetail().isPressed();

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

  uint8_t before = 0, button = 0;
  if (!M5.Power.M5pm1.readRegister(kPm1ButtonCfg1, &before, 1)) {
    Serial.printf("pm1 single reset disable failed: read\n");
  } else if (!M5.Power.M5pm1.writeRegister8(kPm1ButtonCfg1,
                                          before | kPm1SingleResetDisable)) {
    Serial.printf("pm1 single reset disable failed: write\n");
  } else if (!M5.Power.M5pm1.readRegister(kPm1ButtonCfg1, &button, 1)) {
    Serial.printf("pm1 single reset disable failed: verify\n");
  } else if (!(button & kPm1SingleResetDisable)) {
    Serial.printf("pm1 single reset disable failed: verify %02x\n", button);
  } else {
    Serial.printf("pm1 single reset disabled: btn_cfg1=%02x->%02x\n", before, button);
  }

  uint8_t pwrBefore = 0, pwrAfter = 0;
  if (!M5.Power.M5pm1.readRegister(kPm1PwrCfg, &pwrBefore, 1)) {
    Serial.printf("pm1 led disable failed: read\n");
  } else if (!M5.Power.M5pm1.writeRegister8(kPm1PwrCfg,
                                            uint8_t(pwrBefore & ~kPm1LedEn))) {
    Serial.printf("pm1 led disable failed: write\n");
  } else if (!M5.Power.M5pm1.readRegister(kPm1PwrCfg, &pwrAfter, 1)) {
    Serial.printf("pm1 led disable failed: verify\n");
  } else if (pwrAfter & kPm1LedEn) {
    Serial.printf("pm1 led disable failed: verify %02x\n", pwrAfter);
  } else {
    Serial.printf("pm1 led disabled: pwr_cfg=%02x->%02x\n", pwrBefore, pwrAfter);
  }

#if defined(ESP32) || defined(ESP32S3)
  if (M5.Imu.isEnabled()) configureLowPowerImu();
#endif
}

void updateVibration() {
  if (vibration_active &&
      static_cast<int32_t>(millis() - vibration_until_ms) >= 0) {
    M5.Power.setVibration(0);
    vibration_active = false;
  }
}

void update() {
  M5.update();
  updateVibration();
}

bool restUntilInput(uint32_t durationMs) {
#if !defined(ESP32) && !defined(ESP32S3)
  (void)durationMs;
  return false;
#else
  if (durationMs == 0) return true;
  if (esp_sleep_enable_timer_wakeup(uint64_t(durationMs) * 1000) != ESP_OK) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    return false;
  }
  const esp_err_t slept = esp_light_sleep_start();
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

void resetTouchEdge() { previous_touch_down = false; }

bool readAccel(float& x, float& y, float& z, bool& fresh) {
#if !defined(ESP32) && !defined(ESP32S3)
  (void)x;
  (void)y;
  (void)z;
  fresh = false;
  return false;
#else
  if (bmi270_addr != 0) {
    constexpr uint32_t kImuI2cFreq = 400000;
    uint8_t status = 0;
    if (!M5.In_I2C.readRegister(bmi270_addr, 0x03, &status, 1, kImuI2cFreq)) {
      fresh = false;
      return false;
    }
    if ((status & 0x80u) == 0) {
      fresh = false;
      return true;
    }
    uint8_t raw[6] = {};
    if (!M5.In_I2C.readRegister(bmi270_addr, 0x0C, raw, sizeof(raw), kImuI2cFreq)) {
      fresh = false;
      return false;
    }
    constexpr float kAccelScale = 8.0f / 32768.0f;
    const int16_t rx = static_cast<int16_t>(uint16_t(raw[0]) | (uint16_t(raw[1]) << 8));
    const int16_t ry = static_cast<int16_t>(uint16_t(raw[2]) | (uint16_t(raw[3]) << 8));
    const int16_t rz = static_cast<int16_t>(uint16_t(raw[4]) | (uint16_t(raw[5]) << 8));
    x = float(rx) * kAccelScale;
    y = float(ry) * kAccelScale;
    z = float(rz) * kAccelScale;
    fresh = true;
    return true;
  }
  const auto mask = M5.Imu.update();
  fresh = (mask & m5::IMU_Class::sensor_mask_accel) != 0;
  if (fresh) {
    const auto& data = M5.Imu.getImuData();
    x = data.accel.x;
    y = data.accel.y;
    z = data.accel.z;
  }
  return true;
#endif
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

}
