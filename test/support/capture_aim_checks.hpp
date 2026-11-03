#pragma once
#include <M5Unified.h>
#include <vector>

namespace capture_aim_test {
// Below the upper ring's maximum extent (y=363), above the instruction text.
// This band crosses the old charge arc and pulse, but not the ball icon.
inline std::vector<uint16_t> lowerBand() {
  std::vector<uint16_t> pixels;
  for (int y = 368; y <= 380; ++y)
    for (int x = 197; x <= 269; ++x) pixels.push_back(M5.Display.readPixel(x, y));
  return pixels;
}
}  // namespace capture_aim_test
