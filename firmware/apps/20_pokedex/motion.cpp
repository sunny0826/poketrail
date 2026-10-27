#include "motion.hpp"
#include <cmath>

namespace pokedex { namespace motion {
namespace {
const uint32_t kGapMs = 100;
// Recall-first tuning: a step only has to be >=220 ms apart (273 steps/min) and
// may be followed by up to 2.2 s of rest before its candidate is discarded.
const uint32_t kMinStepMs = 220;
const float kValley = 0.97f;
// Evidence required scales down as the impact grows: a faint crest must stay
// above kWeakPeak for ~100 ms, a medium crest is accepted whatever its width,
// and a strong impact confirms on two peaks. Any accepted crest is at least
// weak evidence, so a mixed tier gait confirms on kConfirmPeaks peaks.
const float kWeakPeak = 1.05f;
const float kMediumPeak = 1.08f;
const float kStrongPeak = 1.12f;
const uint8_t kWeakCrestSamples = 5;
const uint8_t kStrongConfirm = 2;
// Starting a cadence from ambiguous crests needs a rhythm, not just three
// excursions: bounded IMU noise can also produce three candidate peaks close
// together. Confirmed cadence keeps the wider tracking tolerance below.
const uint32_t kConfirmLowPct = 65;
const uint32_t kConfirmHighPct = 135;
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
    // Evidence counts monotonically towards weaker tiers: a strong crest also
    // proves medium and weak cadence, a medium crest also proves weak cadence.
    // Counting the whole pending window, instead of a contiguous medium/strong
    // suffix, keeps alternating tiers from being dropped before confirmation.
    uint8_t strongCount = 0;
    for (uint8_t i = 0; i < pendingCount_; ++i)
      if (pendingTier_[i] >= 2) ++strongCount;
    if (strongCount >= kStrongConfirm ||
        (pendingCount_ >= kConfirmPeaks && regularRhythm())) {
      for (uint8_t i = 0; i < pendingCount_; ++i) result.events[result.count++] = pending_[i];
      pendingCount_ = 0;
      confirmed_ = true;
      return;
    }
    if (pendingCount_ < kConfirmPeaks) return;
    // Irregular candidate chain: keep the newest peaks and re-check the rest.
    for (uint8_t i = 1; i < pendingCount_; ++i) {
      pending_[i - 1] = pending_[i];
      pendingTier_[i - 1] = pendingTier_[i];
    }
    --pendingCount_;
  }
}
bool Detector::regularRhythm() const {
  const uint32_t first = pending_[1].monotonicMs - pending_[0].monotonicMs;
  for (uint8_t i = 2; i < pendingCount_; ++i) {
    const uint32_t gap = pending_[i].monotonicMs - pending_[i - 1].monotonicMs;
    if (gap * 100u < first * kConfirmLowPct || gap * 100u > first * kConfirmHighPct) return false;
  }
  return true;
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
