#pragma once
#include <stdint.h>

namespace pokedex { namespace motion {
// Cadence limits shared with clock attribution.
constexpr uint32_t kMaxStepMs = 2200;
// Any accepted crest is weak evidence; three of them confirm only as a regular
// rhythm, while two strong peaks confirm on their own.
constexpr uint8_t kConfirmPeaks = 3;
// Samples further apart than this are reported as a quality gap and restart the
// detector, so a live cadence always advances in smaller steps.
constexpr uint32_t kMaxSampleGapMs = 100;
// A crest is accepted at the first valley after its apex, or at the latest one
// cadence window after the apex when the signal never returns to the valley.
// Because that deadline is only noticed by a sample, the release can trail the
// apex by up to one more tolerated sample gap. Consecutive peaks of a batch are
// at most one cadence window apart, so the oldest event of a batch trails its
// confirming sample by at most kConfirmPeaks windows plus that gap.
constexpr uint32_t kMaxConfirmationDelayMs = kMaxStepMs * kConfirmPeaks + kMaxSampleGapMs;
struct StepEvent { uint32_t monotonicMs; uint16_t tag; };
struct StepBatch {
  StepEvent events[kConfirmPeaks];
  uint8_t count;
  bool qualityGap;
  bool suppressed;
};
class Detector {
 public:
  Detector();
  StepBatch sample(float xG, float yG, float zG, uint32_t monotonicMs, uint16_t tag = 0);
  void reset();
  void suppressUntil(uint32_t monotonicMs);
 private:
  void clearCadence();
  void acceptPeak(uint32_t at, float peak, uint8_t crest, StepBatch& result);
  bool regularRhythm() const;
  bool hasSample_, armed_, tracking_, confirmed_, suppression_;
  uint32_t lastSample_, peakAt_, lastPeak_, interval_, suppressEnd_;
  float filtered_, peak_;
  StepEvent pending_[kConfirmPeaks];
  int8_t pendingTier_[kConfirmPeaks];
  uint16_t peakTag_;
  uint8_t pendingCount_, crestSamples_;
};
} }
