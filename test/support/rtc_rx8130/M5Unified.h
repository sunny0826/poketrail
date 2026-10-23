#pragma once
#include "utility/rtc/RX8130_Class.hpp"
void rtcTestDelay(unsigned ms);
namespace m5 {
class M5Unified { public: static void delay(unsigned ms) { rtcTestDelay(ms); } };
}
