#pragma once
#include "Arduino.h"
#include "utility/rtc/RX8130_Class.hpp"
void rtcTestDelay(unsigned ms);
namespace fonts { static const int Font2 = 0; }
static const int bottom_center = 0, TFT_CYAN = 0, TFT_BLACK = 0;
namespace m5 {
extern unsigned uncheckedReads;
class RTC_Class {
 public:
  RTC_Class() = default;
  bool isEnabled() const { return base_ != nullptr; }
  bool begin(I2C_Class* bus = nullptr, int = 0) {
    owned_ = RX8130_Class();
    base_ = owned_.begin(bus) ? &owned_ : nullptr;
    return isEnabled();
  }
  void setSystemTimeFromRtc() {
    ++uncheckedReads;
    rtc_date_t date; rtc_time_t time;
    if (base_) base_->getDateTime(&date, &time);
  }
  void disableIRQ() { if (base_) base_->disableIRQ(); }
 private:
  RX8130_Class owned_;
  RTC_Base* base_ = nullptr;
};
class M5Unified {
 public:
  struct config_t {
    unsigned serial_baudrate = 0;
    bool clear_display = true, internal_imu = true, internal_rtc = true;
    bool internal_spk = true, output_power = true;
    bool external_rtc = false, external_imu = false, disable_rtc_irq = true;
  };
  struct DisplayShell {
    void setRotation(int) {} void setBrightness(int) {}
    void setTextWrap(bool) {} void setSwapBytes(bool) {}
    int width() const { return 468; } int height() const { return 468; }
    void setTextDatum(int) {} void setFont(const int*) {}
    void setTextColor(int,int) {} void drawString(const char*,int,int) {}
  } Display;
  struct SpeakerShell { void setVolume(int) {} void tone(int,int) {} } Speaker;
  struct PowerShell { void setVibration(int) {} } Power;
  struct Detail { int x=0,y=0; bool isPressed() const { return false; } };
  struct TouchShell { Detail getDetail() const { return {}; } } Touch;
  struct ImuShell {
    bool enabled = false;
    bool begin(I2C_Class*, int=0) { enabled=true; return true; }
    bool isEnabled() const { return enabled; }
  } Imu;
  I2C_Class& In_I2C = m5::In_I2C;
  I2C_Class& Ex_I2C = m5::Ex_I2C;
  RTC_Class Rtc;
  config_t lastConfig;
  config_t config() const { return {}; }
  void begin(const config_t& cfg) {
    lastConfig=cfg;
    In_I2C.setPort(0,1,2);
    _begin_rtc_imu(cfg);
  }
  bool _begin_rtc_imu(const config_t&);
  int getBoard() const { return 30; }
  void update() {}
  static void delay(unsigned ms) { rtcTestDelay(ms); }
};
}
extern m5::M5Unified M5;
