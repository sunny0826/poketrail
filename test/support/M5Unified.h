#pragma once

#include "Arduino.h"
#include "HostDisplay.hpp"
#include <utility/Button_Class.hpp>

struct HostTouchDetail {
  int32_t x = 0, y = 0, base_x = 0, base_y = 0;
  bool pressed = false, released = false, clicked = false;
  bool isPressed() const { return pressed; }
  bool wasReleased() const { return released; }
  bool wasClicked() const { return clicked; }
};

struct HostM5 {
  struct Config {
    uint32_t serial_baudrate = 0;
    bool clear_display = false, internal_imu = false, internal_rtc = false;
    bool external_rtc = false, disable_rtc_irq = true;
    bool internal_spk = false, output_power = false;
  };
  struct TouchStub {
    HostTouchDetail detail;
    HostTouchDetail getDetail() const { return detail; }
  } Touch;
  struct SpeakerStub {
    void setVolume(uint8_t) {}
    void tone(uint16_t, uint16_t) {}
  } Speaker;
  struct Pm1Stub {
    uint8_t button = 0;
    uint8_t hold_cfg = 0;
    bool readRegister(uint8_t reg, uint8_t* out, size_t length) {
      if (length != 1) return false;
      if (reg == 0x48) {
        *out = button;
        button &= ~0x80;
        return true;
      }
      if (reg == 0x07) {
        *out = hold_cfg;
        return true;
      }
      return false;
    }
    bool writeRegister8(uint8_t reg, uint8_t value) {
      if (reg != 0x07) return false;
      hold_cfg = value;
      return true;
    }
  };
  struct PowerStub {
    uint8_t vibration = 0;
    Pm1Stub M5pm1;
    void setVibration(uint8_t value) { vibration = value; }
  } Power;

  m5::Button_Class BtnA, BtnB;
  bool button_a_down = false, button_b_down = false;
  HostDisplay Display;
  Config config() const { return {}; }
  void begin(const Config&) { Display.createSprite(468, 468); }
  // A short press: the pressed flag survives until the next poll, the level is
  // already released, matching PM1 0x48 after a sub-poll click.
  void powerClick() { Power.M5pm1.button |= 0x80; }
  void powerDown(bool down) { Power.M5pm1.button = down ? 0x81 : 0x00; }
  void update() {
    BtnA.setRawState(millis(), button_a_down);
    BtnB.setRawState(millis(), button_b_down);
  }
  int getBoard() const { return 30; }
};

extern HostM5 M5;
