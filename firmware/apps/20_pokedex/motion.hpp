#pragma once
#include <stdint.h>

namespace pokedex { namespace motion {
struct StepEvent { uint32_t monotonicMs; uint16_t tag; };
struct StepBatch {
  StepEvent events[4];
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
  bool hasSample_, armed_, tracking_, confirmed_, suppression_;
  uint32_t lastSample_, peakAt_, lastPeak_, interval_, suppressEnd_;
  float filtered_, peak_;
  StepEvent pending_[4];
  int8_t pendingTier_[4];
  uint16_t peakTag_;
  uint8_t pendingCount_, crestSamples_;
};
} }
