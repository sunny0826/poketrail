#include "board.hpp"

#include <stdio.h>
#if defined(ESP32) || defined(ESP32S3)
#include <esp_sleep.h>
#include <esp_timer.h>
#endif

namespace {

enum class VibrationStage : uint8_t { Idle, Pulse1, Gap, Pulse2 };

struct VibrationState {
  VibrationStage stage = VibrationStage::Idle;
  uint32_t until_ms = 0;
  uint16_t gap_ms = 0;
  uint16_t on2_ms = 0;
  uint8_t level = 0;
  bool motor_on = false;
};

VibrationState vibration_state;
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

constexpr uint32_t kButtonDebounceMs = 10;
constexpr uint8_t kBtnAGpio = 2;
constexpr uint8_t kBtnBGpio = 1;
static_assert(GPIO_NUM_2 == kBtnAGpio && GPIO_NUM_1 == kBtnBGpio, "C152 button GPIOs");

struct ButtonEdgeLatch {
  uint32_t lastEdgeMs = 0;
  uint32_t pressMs = 0;
  uint32_t releaseMs = 0;
  bool hasEdge = false;
  bool lastDown = false;
  bool pressPending = false;
  bool releasePending = false;
};

portMUX_TYPE button_mux = portMUX_INITIALIZER_UNLOCKED;
ButtonEdgeLatch btn_a_latch;
ButtonEdgeLatch btn_b_latch;
bool button_irq_attached = false;

inline uint32_t IRAM_ATTR readBoardMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

inline bool IRAM_ATTR readPinDown(uint8_t gpio) {
  return ((REG_READ(GPIO_IN_REG) >> gpio) & 1u) == 0;
}

void IRAM_ATTR recordButtonEdge(ButtonEdgeLatch& latch, uint8_t gpio) {
  const uint32_t nowMs = readBoardMs();
  const bool down = readPinDown(gpio);
  portENTER_CRITICAL_ISR(&button_mux);
  if (down != latch.lastDown &&
      (!latch.hasEdge || static_cast<uint32_t>(nowMs - latch.lastEdgeMs) >= kButtonDebounceMs)) {
    latch.hasEdge = true;
    latch.lastEdgeMs = nowMs;
    latch.lastDown = down;
    if (down) {
      latch.pressMs = nowMs;
      latch.pressPending = true;
    } else {
      latch.releaseMs = nowMs;
      latch.releasePending = true;
    }
  }
  portEXIT_CRITICAL_ISR(&button_mux);
}

void IRAM_ATTR onBtnAEdgeIsr() { recordButtonEdge(btn_a_latch, kBtnAGpio); }
void IRAM_ATTR onBtnBEdgeIsr() { recordButtonEdge(btn_b_latch, kBtnBGpio); }

void syncButtonLatches() {
  if (!button_irq_attached) return;
  const uint32_t nowMs = readBoardMs();
  const bool aDown = readPinDown(kBtnAGpio);
  const bool bDown = readPinDown(kBtnBGpio);
  portENTER_CRITICAL(&button_mux);
  if (btn_a_latch.hasEdge &&
      static_cast<uint32_t>(nowMs - btn_a_latch.lastEdgeMs) >= kButtonDebounceMs) {
    btn_a_latch.lastDown = aDown;
  }
  if (btn_b_latch.hasEdge &&
      static_cast<uint32_t>(nowMs - btn_b_latch.lastEdgeMs) >= kButtonDebounceMs) {
    btn_b_latch.lastDown = bDown;
  }
  portEXIT_CRITICAL(&button_mux);
}

void initButtonEdgeLatches() {
  if (M5.getBoard() != m5::board_t::board_M5StopWatch) return;
  if (button_irq_attached) {
    detachInterrupt(kBtnAGpio);
    detachInterrupt(kBtnBGpio);
    button_irq_attached = false;
  }
  portENTER_CRITICAL(&button_mux);
  btn_a_latch = ButtonEdgeLatch();
  btn_b_latch = ButtonEdgeLatch();
  btn_a_latch.lastDown = readPinDown(kBtnAGpio);
  btn_b_latch.lastDown = readPinDown(kBtnBGpio);
  portEXIT_CRITICAL(&button_mux);
  attachInterrupt(kBtnAGpio, onBtnAEdgeIsr, CHANGE);
  attachInterrupt(kBtnBGpio, onBtnBEdgeIsr, CHANGE);
  button_irq_attached = true;
}

bool consumeLatchAgeMs(ButtonEdgeLatch& latch, bool release, uint32_t maxAgeMs, uint32_t& ageMs) {
  if (!button_irq_attached) return false;
  uint32_t elapsed = 0;
  bool valid = false;
  portENTER_CRITICAL(&button_mux);
  bool& pending = release ? latch.releasePending : latch.pressPending;
  if (pending) {
    pending = false;
    const uint32_t latchedMs = release ? latch.releaseMs : latch.pressMs;
    elapsed = static_cast<uint32_t>(readBoardMs() - latchedMs);
    valid = elapsed <= maxAgeMs;
  }
  portEXIT_CRITICAL(&button_mux);
  if (!valid) return false;
  ageMs = elapsed;
  return true;
}
#endif

}

namespace c152 {

void begin(const char* title) {
  vibration_state = VibrationState();
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
#if defined(ESP32) || defined(ESP32S3)
  M5.Touch.setFlickThresh(20);
  M5.Touch.setHoldThresh(650);
#endif

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
  initButtonEdgeLatches();
#endif
}

void updateVibration() {
  if (vibration_state.stage == VibrationStage::Idle) return;
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - vibration_state.until_ms) < 0) return;

  if (vibration_state.stage == VibrationStage::Pulse1 &&
      vibration_state.gap_ms > 0 && vibration_state.on2_ms > 0) {
    M5.Power.setVibration(0);
    vibration_state.motor_on = false;
    vibration_state.stage = VibrationStage::Gap;
    vibration_state.until_ms = now + vibration_state.gap_ms;
    return;
  }

  if (vibration_state.stage == VibrationStage::Gap &&
      vibration_state.on2_ms > 0) {
    M5.Power.setVibration(vibration_state.level);
    vibration_state.motor_on = true;
    vibration_state.stage = VibrationStage::Pulse2;
    vibration_state.until_ms = now + vibration_state.on2_ms;
    vibration_state.gap_ms = 0;
    vibration_state.on2_ms = 0;
    return;
  }

  M5.Power.setVibration(0);
  vibration_state = VibrationState();
}

void update() {
  M5.update();
#if defined(ESP32) || defined(ESP32S3)
  syncButtonLatches();
#endif
  updateVibration();
}

uint32_t boardMs() {
#if defined(ESP32) || defined(ESP32S3)
  return readBoardMs();
#else
  return millis();
#endif
}

bool consumeBtnAPressAgeMs(uint32_t maxAgeMs, uint32_t& ageMs) {
#if defined(ESP32) || defined(ESP32S3)
  return consumeLatchAgeMs(btn_a_latch, false, maxAgeMs, ageMs);
#else
  (void)maxAgeMs;
  (void)ageMs;
  return false;
#endif
}

bool consumeBtnBPressAgeMs(uint32_t maxAgeMs, uint32_t& ageMs) {
#if defined(ESP32) || defined(ESP32S3)
  return consumeLatchAgeMs(btn_b_latch, false, maxAgeMs, ageMs);
#else
  (void)maxAgeMs;
  (void)ageMs;
  return false;
#endif
}

bool consumeBtnBReleaseAgeMs(uint32_t maxAgeMs, uint32_t& ageMs) {
#if defined(ESP32) || defined(ESP32S3)
  return consumeLatchAgeMs(btn_b_latch, true, maxAgeMs, ageMs);
#else
  (void)maxAgeMs;
  (void)ageMs;
  return false;
#endif
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

void stopVibration() {
  if (vibration_state.stage == VibrationStage::Idle &&
      !vibration_state.motor_on) {
    return;
  }
  if (vibration_state.motor_on) {
    M5.Power.setVibration(0);
  }
  vibration_state = VibrationState();
}

void feedback(uint16_t frequency_hz, uint16_t duration_ms,
              uint8_t vibration) {
  if (frequency_hz > 0) {
    M5.Speaker.tone(frequency_hz, duration_ms);
  }
  if (vibration == 0) return;
  if (duration_ms == 0) {
    stopVibration();
    return;
  }
  M5.Power.setVibration(vibration);
  vibration_state = VibrationState();
  vibration_state.stage = VibrationStage::Pulse1;
  vibration_state.until_ms = millis() + duration_ms;
  vibration_state.level = vibration;
  vibration_state.motor_on = true;
}

void feedbackPulse(uint16_t on1_ms, uint16_t gap_ms, uint16_t on2_ms,
                   uint8_t vibration) {
  if (vibration == 0) {
    stopVibration();
    return;
  }
  if (on1_ms == 0) {
    feedback(0, on2_ms, vibration);
    return;
  }
  if (on2_ms == 0) {
    feedback(0, on1_ms, vibration);
    return;
  }
  if (gap_ms == 0) {
    const uint32_t combined = uint32_t(on1_ms) + uint32_t(on2_ms);
    feedback(0, combined > 0xffffu ? uint16_t(0xffffu) : uint16_t(combined),
             vibration);
    return;
  }
  M5.Power.setVibration(vibration);
  vibration_state = VibrationState();
  vibration_state.stage = VibrationStage::Pulse1;
  vibration_state.until_ms = millis() + on1_ms;
  vibration_state.gap_ms = gap_ms;
  vibration_state.on2_ms = on2_ms;
  vibration_state.level = vibration;
  vibration_state.motor_on = true;
}

bool vibrationActive() {
  return vibration_state.stage != VibrationStage::Idle;
}

}
