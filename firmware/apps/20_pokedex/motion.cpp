#include "motion.hpp"
#include <cmath>

namespace pokedex { namespace motion {
namespace {
const uint32_t kGapMs = 100;
// Recall-first tuning: a step only has to be >=220 ms apart (273 steps/min) and
// may be followed by up to 2.2 s of rest before its candidate is discarded.
const uint32_t kMinStepMs = 220;
const uint32_t kMaxStepMs = 2200;
const float kValley = 0.97f;
// Evidence required scales down as the impact grows: a faint crest must stay
// above kWeakPeak for ~100 ms, a medium crest needs a longer run, and a strong
// impact confirms on two peaks. Weak pocket harmonics clear the medium tier.
const float kWeakPeak = 1.05f;
const float kMediumPeak = 1.08f;
const float kStrongPeak = 1.12f;
const uint8_t kWeakCrestSamples = 5;
const uint8_t kStrongConfirm = 2;
const uint8_t kMediumConfirm = 4;
const uint8_t kWeakConfirm = 3;
const uint8_t kPendingMax = 4;
const uint32_t kIrregularLowPct = 60;
const uint32_t kIrregularHighPct = 165;
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
  crestSamples_ = 0;
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
void Detector::acceptPeak(uint32_t at, float peak, uint8_t crest, StepBatch& result) {
  const int8_t tier = peak >= kStrongPeak ? 2 : (peak >= kMediumPeak ? 1 : 0);
  if (tier == 0 && crest < kWeakCrestSamples) return;
  if (pendingCount_ || confirmed_) {
    const uint32_t elapsed = at - lastPeak_;
    if (elapsed < kMinStepMs) return;
    const bool irregular = interval_ &&
        (elapsed * 100u < interval_ * kIrregularLowPct ||
         elapsed * 100u > interval_ * kIrregularHighPct);
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
    result.events[result.count++] = {at, peakTag_};
    return;
  }
  pending_[pendingCount_] = {at, peakTag_};
  pendingTier_[pendingCount_++] = tier;
  for (;;) {
    uint8_t strongRun = 0, mediumRun = 0;
    for (int8_t i = (int8_t)pendingCount_ - 1; i >= 0 && pendingTier_[i] >= 1; --i) {
      ++mediumRun;
      if (pendingTier_[i] >= 2) ++strongRun;
    }
    bool allWeak = true;
    for (uint8_t i = 0; i < pendingCount_; ++i) allWeak = allWeak && pendingTier_[i] == 0;
    if (strongRun >= kStrongConfirm || mediumRun >= kMediumConfirm ||
        (allWeak && pendingCount_ >= kWeakConfirm)) {
      for (uint8_t i = 0; i < pendingCount_; ++i) result.events[result.count++] = pending_[i];
      pendingCount_ = 0;
      confirmed_ = true;
      return;
    }
    if (pendingCount_ < kPendingMax) return;
    // Mixed strong/weak evidence that cannot confirm: keep the newest peaks.
    for (uint8_t i = 1; i < pendingCount_; ++i) {
      pending_[i - 1] = pending_[i];
      pendingTier_[i - 1] = pendingTier_[i];
    }
    --pendingCount_;
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
  // 0.8 passes the ~9Hz footstep impact carried in pockets and bags; 0.5
  // attenuated it below the valley threshold and lost most pocket steps.
  filtered_ += 0.8f * (magnitude - filtered_);
  if (!tracking_ && (confirmed_ || pendingCount_) && now - lastPeak_ > kMaxStepMs)
    clearCadence();
  if (filtered_ >= kWeakPeak && (armed_ || tracking_) && crestSamples_ < kWeakCrestSamples)
    ++crestSamples_;
  if (filtered_ <= kValley) {
    if (tracking_) acceptPeak(peakAt_, peak_, crestSamples_, result);
    armed_ = true;
    tracking_ = false;
    crestSamples_ = 0;
  } else if (armed_ && filtered_ >= kWeakPeak) {
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
