#include "test_mode.hpp"

namespace pokedex { namespace adventure {

void TestMode::beginSimulation(uint32_t nowMs, int64_t utcSeconds, int16_t offsetMinutes) {
  clockStarted = true;
  clockBaseMs = nowMs;
  clockBaseUtc = utcSeconds;
  clockOffsetMinutes = offsetMinutes;
}

void TestMode::endSimulation() {
  clockStarted = false;
  clockBaseMs = 0;
  clockBaseUtc = 0;
  clockOffsetMinutes = 480;
}

int64_t TestMode::utcAt(uint32_t nowMs) const {
  if (!clockStarted) return 0;
  return clockBaseUtc + int64_t(int32_t(nowMs - clockBaseMs)) / 1000;
}

void TestMode::setUtc(uint32_t nowMs, int64_t utcSeconds) {
  beginSimulation(nowMs, utcSeconds, clockOffsetMinutes);
}

void TestMode::setOffset(int16_t offsetMinutes) {
  clockOffsetMinutes = offsetMinutes;
}

} }
