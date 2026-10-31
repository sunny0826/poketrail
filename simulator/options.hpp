#pragma once
#include <cstdint>
#include <ctime>
#include <functional>
#include <random>
#include <string>

namespace simulator {
enum class Fault { None, BeforeWrite, AfterWrite, ReadBack, PartialWrite, ClockWrite, SlowWrite };
struct Options {
  uint32_t seed = std::random_device{}();
  std::function<int64_t()> wallClock = [] { return int64_t(std::time(nullptr)); };
  std::function<void(const std::string&)> error;
  std::function<void(unsigned)> windowEvent;
  bool isolateHostFocus = false;
};
}
