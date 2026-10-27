#pragma once
#include "../../firmware/apps/20_pokedex/motion.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

unsigned checks = 0;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct Feed {
  pokedex::motion::Detector detector;
  uint32_t now;
  unsigned gaps;
  uint16_t tag = 0;
  float lastFiltered = 1;
  uint32_t maxAge = 0;
  std::vector<uint32_t> steps;
  std::vector<uint16_t> tags;
  explicit Feed(uint32_t start = 0) : now(start), gaps(0) {}
  void record(const pokedex::motion::StepBatch& b) {
    check(b.count <= pokedex::motion::kConfirmPeaks, "bounded batch");
    for (unsigned i = 0; i < b.count; ++i) {
      const uint32_t age = now - b.events[i].monotonicMs;
      if (age > maxAge) maxAge = age;
      check(age <= pokedex::motion::kMaxConfirmationDelayMs,
            "confirmed peak stays inside the clock attribution window");
      if (!steps.empty()) check(b.events[i].monotonicMs - steps.back() >= 220,
                                "ordered distinct events, including wrap");
      steps.push_back(b.events[i].monotonicMs);
      tags.push_back(b.events[i].tag);
    }
    gaps += b.qualityGap;
    now += 20;
  }
  pokedex::motion::StepBatch sample(float magnitude, int axis = 0) {
    const float x = axis == 0 ? magnitude : (axis == 3 ? magnitude * 0.6f : 0);
    const float y = axis == 1 ? -magnitude : (axis == 3 ? magnitude * 0.8f : 0);
    const float z = axis == 2 ? magnitude : 0;
    pokedex::motion::StepBatch b = detector.sample(x, y, z, now, tag);
    record(b);
    return b;
  }
  // 反解一阶滤波，直接指定 filtered_（reviewer 复现方式），用于精确驱动
  // “末峰后长时间不落谷”的场景；不能与 sample() 混用。
  void sampleFiltered(float filtered) {
    const float raw = (filtered - 0.2f * lastFiltered) / 0.8f;
    lastFiltered = filtered;
    record(detector.sample(raw, 0, 0, now));
  }
  void rest(unsigned ms, int axis = 0) {
    for (unsigned i = 0; i < ms; i += 20) sample(1, axis);
  }
  void cycle(unsigned period = 500, int axis = 0) { cycleAmp(period, 0.30f, axis); }
  void cycleAmp(unsigned period, float amplitude, int axis = 0) {
    for (unsigned i = 0; i < period; i += 20)
      sample(1.0f - amplitude * std::cos(6.28318530718f * float(i) / period), axis);
  }
};
