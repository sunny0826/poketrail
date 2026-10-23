#include "motion.hpp"
#include <cmath>

namespace pokedex { namespace motion {
namespace {
const uint32_t kGapMs = 100;
const uint32_t kMinStepMs = 280;
const uint32_t kMaxStepMs = 1500;
bool before(uint32_t now, uint32_t end) {
  return (end - now) != 0 && (end - now) < 0x80000000u;
}
}
Detector::Detector() : suppression_(false), suppressEnd_(0) { reset(); }
void Detector::clearCadence() {
  armed_ = tracking_ = confirmed_ = false;
  pendingCount_ = 0;
  interval_ = 0;
  lastPeak_ = peakAt_ = 0;
  peak_ = 0;
  peakTag_ = 0;
}
void Detector::reset() {
  hasSample_ = false;
  lastSample_ = 0;
  filtered_ = 1.0f;
  clearCadence();
}
void Detector::suppressUntil(uint32_t end) {
  if (!suppression_ || before(suppressEnd_, end)) suppressEnd_ = end;
  suppression_ = true;
  reset();
}
void Detector::acceptPeak(uint32_t at, uint16_t tag, StepBatch& result) {
  if (pendingCount_ || confirmed_) {
    const uint32_t elapsed = at - lastPeak_;
    if (elapsed < kMinStepMs) return;
    const bool irregular = interval_ &&
        (elapsed * 100u < interval_ * 65u || elapsed * 100u > interval_ * 135u);
    if (elapsed > kMaxStepMs || irregular) {
      confirmed_ = false;
      pendingCount_ = 0;
      interval_ = 0;
    } else {
      interval_ = elapsed;
    }
  }
  lastPeak_ = at;
  if (confirmed_) {
    result.events[0] = {at, tag};
    result.count = 1;
    return;
  }
  pending_[pendingCount_++] = {at, tag};
  if (pendingCount_ == 3) {
    for (uint8_t i = 0; i < 3; ++i) result.events[i] = pending_[i];
    result.count = 3;
    pendingCount_ = 0;
    confirmed_ = true;
  }
}
StepBatch Detector::sample(float x, float y, float z, uint32_t now, uint16_t tag) {
  StepBatch result = {};
  if (suppression_) {
    if (before(now, suppressEnd_)) {
      result.suppressed = true;
      return result;
    }
    suppression_ = false;
    reset();
  }
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
      || std::fabs(x) > 4 || std::fabs(y) > 4 || std::fabs(z) > 4) {
    reset();
    result.qualityGap = true;
    return result;
  }
  const float magnitude = std::sqrt(x*x + y*y + z*z);
  if (magnitude < 0.1f || magnitude > 4.0f) {
    reset();
    result.qualityGap = true;
    return result;
  }
  if (hasSample_) {
    const uint32_t elapsed = now - lastSample_;
    if (elapsed == 0 || elapsed >= kGapMs) {
      reset();
      result.qualityGap = true;
    }
  }
  if (!hasSample_) {
    hasSample_ = true;
    lastSample_ = now;
    filtered_ = magnitude;
    return result;
  }
  lastSample_ = now;
  filtered_ += 0.5f * (magnitude - filtered_);
  if (!tracking_ && (confirmed_ || pendingCount_) && now - lastPeak_ > kMaxStepMs)
    clearCadence();
  if (filtered_ <= 0.94f) {
    if (tracking_) acceptPeak(peakAt_, peakTag_, result);
    armed_ = true;
    tracking_ = false;
  } else if (armed_ && filtered_ >= 1.12f) {
    if (!tracking_ || filtered_ > peak_) {
      peak_ = filtered_;
      peakAt_ = now;
      peakTag_ = tag;
    }
    tracking_ = true;
  }
  return result;
}
} }
