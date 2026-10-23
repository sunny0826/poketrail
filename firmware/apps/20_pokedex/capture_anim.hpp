#pragma once
#include <cstdint>
#include "adventure_model.hpp"

// 捕获动画是纯表现层：只读已提交存档的 attempts 记录，不改变任何玩法状态。
// 所有位置与阶段都是 (startMs, now) 的纯函数，息屏唤醒或卡顿只会跳帧。
namespace pokedex { namespace ui {

constexpr uint16_t kGold = 0xFEA0;
constexpr int kBallX = 233;
constexpr int kHandY = 344;
constexpr int kGroundY = 300;
constexpr uint32_t kChargeFullMs = 800;
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
  uint8_t wiggleBreak = 0;  // 失败时球在第 1+roll%3 次摇摆的中点爆开
};

struct CaptureFrame {
  CaptureStage stage = CaptureStage::Done;
  float t = 0;         // 阶段内进度 0..1
  uint8_t wiggle = 0;  // 当前第几次摇摆（从 1 计）
};

// 触感与提示事件，Runtime 在编码变化时触发一次震动
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
uint8_t captureChargeLevel(uint32_t heldMs);

} }
