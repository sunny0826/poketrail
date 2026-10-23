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
  struct PowerStub {
    uint8_t vibration = 0;
    void setVibration(uint8_t value) { vibration = value; }
  } Power;

  m5::Button_Class BtnA, BtnB;
  bool button_a_down = false, button_b_down = false;
  HostDisplay Display;
  Config config() const { return {}; }
  void begin(const Config&) { Display.createSprite(468, 468); }
  void update() {
    BtnA.setRawState(millis(), button_a_down);
    BtnB.setRawState(millis(), button_b_down);
  }
  int getBoard() const { return 30; }
};

extern HostM5 M5;
