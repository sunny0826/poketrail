#pragma once
#include <cstdint>
#include "adventure_model.hpp"

namespace pokedex { namespace ui {

constexpr int kBallX = 233;
constexpr int kHandY = 344;
constexpr int kGroundY = 300;
constexpr uint32_t kFlightMs = 450;
constexpr uint32_t kFlashMs = 120;
constexpr uint32_t kDropMs = 500;
constexpr uint32_t kWiggleMs = 500;
constexpr uint32_t kWiggleCount = 3;
constexpr uint32_t kRevealMs = 600;

enum class CaptureStage : uint8_t { Flight, Flash, Drop, Wiggle, Reveal, Done };

struct CaptureAnim {
  bool active = false;
  uint32_t startMs = 0;
  adventure::Timing timing = adventure::Timing::Miss;
  bool captured = false;
  uint8_t wiggleBreak = 0;
};

struct CaptureFrame {
  CaptureStage stage = CaptureStage::Done;
  float t = 0;
  uint8_t wiggle = 0;
};

enum class CaptureCue : uint8_t {
  None, Throw, Land1, Land2, Wiggle1, Wiggle2, Wiggle3, BreakOut, Success1, Success2, Success3
};

uint32_t captureWiggleMs(const CaptureAnim& anim);
uint32_t captureTotalMs(const CaptureAnim& anim);
CaptureFrame captureFrameAt(const CaptureAnim& anim, uint32_t now);
CaptureCue captureCueAt(const CaptureAnim& anim, uint32_t now);
void captureSkip(CaptureAnim& anim, uint32_t now);

void captureFlightPos(float t, int& x, int& y);
int captureDropY(float t);
float captureWiggleAngle(float t);
void captureStarPos(unsigned index, float t, int& x, int& y);

} }
