#include "capture_anim.hpp"
#include <cmath>

namespace pokedex { namespace ui {
namespace {
constexpr float kPi = 3.14159265f;
float easeOut(float t) { const float u = 1 - t; return 1 - u * u * u; }
}

uint32_t captureWiggleMs(const CaptureAnim& anim) {
  if (anim.captured) return kWiggleCount * kWiggleMs;
  const uint8_t b = anim.wiggleBreak < 1 ? 1 : anim.wiggleBreak > 3 ? 3 : anim.wiggleBreak;
  return (b - 1) * kWiggleMs + kWiggleMs / 2;
}

uint32_t captureTotalMs(const CaptureAnim& anim) {
  const uint32_t hold = anim.captured ? 1000 : 400;
  return kFlightMs + kFlashMs + kDropMs + captureWiggleMs(anim) + kRevealMs + hold;
}

CaptureFrame captureFrameAt(const CaptureAnim& anim, uint32_t now) {
  CaptureFrame f;
  if (!anim.active) return f;
  const uint32_t elapsed = now - anim.startMs;
  uint32_t base = 0;
  const uint32_t segments[] = {kFlightMs, kFlashMs, kDropMs, captureWiggleMs(anim), kRevealMs};
  const CaptureStage stages[] = {CaptureStage::Flight, CaptureStage::Flash, CaptureStage::Drop,
                                 CaptureStage::Wiggle, CaptureStage::Reveal};
  for (unsigned i = 0; i < 5; ++i) {
    if (elapsed < base + segments[i]) {
      f.stage = stages[i];
      f.t = float(elapsed - base) / segments[i];
      if (f.stage == CaptureStage::Wiggle) {
        f.wiggle = uint8_t(1 + (elapsed - base) / kWiggleMs);
        f.t = float((elapsed - base) % kWiggleMs) / float(kWiggleMs);
      }
      return f;
    }
    base += segments[i];
  }
  if (elapsed < captureTotalMs(anim)) {
    f.stage = CaptureStage::Reveal;
    f.t = 1.0f;
  }
  return f;
}

CaptureCue captureCueAt(const CaptureAnim& anim, uint32_t now) {
  const auto f = captureFrameAt(anim, now);
  switch (f.stage) {
    case CaptureStage::Flight: return CaptureCue::Throw;
    case CaptureStage::Drop: return f.t < 0.5f ? CaptureCue::Land1 : CaptureCue::Land2;
    case CaptureStage::Wiggle:
      if (!anim.captured && f.wiggle == anim.wiggleBreak && f.t >= 0.5f) return CaptureCue::BreakOut;
      return CaptureCue(uint8_t(CaptureCue::Wiggle1) + f.wiggle - 1);
    case CaptureStage::Reveal:
      if (!anim.captured) return CaptureCue::BreakOut;
      return f.t < 0.34f ? CaptureCue::Success1 : f.t < 0.67f ? CaptureCue::Success2 : CaptureCue::Success3;
    default: return CaptureCue::None;
  }
}

void captureSkip(CaptureAnim& anim, uint32_t now) {
  const auto f = captureFrameAt(anim, now);
  if (f.stage == CaptureStage::Done) return;
  const uint32_t revealStart = kFlightMs + kFlashMs + kDropMs + captureWiggleMs(anim);
  if (f.stage != CaptureStage::Reveal) anim.startMs = now - revealStart;
  else anim.startMs = now - captureTotalMs(anim);
}

void captureFlightPos(float t, int& x, int& y) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  x = kBallX + int(36.0f * std::sin(kPi * t));
  y = kHandY + int((233 - kHandY) * easeOut(t));
}

int captureDropY(float t) {
  if (t < 0.25f) {
    const float u = t / 0.25f;
    return 233 + int(67.0f * u * u);
  }
  if (t < 0.625f) {
    const float u = (t - 0.25f) / 0.375f;
    return kGroundY - int(24.0f * std::sin(kPi * u));
  }
  const float u = (t - 0.625f) / 0.375f;
  return kGroundY - int(10.0f * std::sin(kPi * u));
}

float captureWiggleAngle(float t) { return 20.0f * std::sin(2 * kPi * t); }

void captureStarPos(unsigned index, float t, int& x, int& y) {
  const float angle = -kPi / 2 + float(index % 6) * kPi / 3;
  const float r = 20.0f + 90.0f * (t < 0 ? 0 : t > 1 ? 1 : t);
  x = 233 + int(r * std::cos(angle));
  y = 250 + int(r * std::sin(angle));
}

uint8_t captureChargeLevel(uint32_t heldMs) {
  if (heldMs >= kChargeFullMs) return 2;
  return heldMs >= kChargeFullMs / 2 ? 1 : 0;
}

} }
