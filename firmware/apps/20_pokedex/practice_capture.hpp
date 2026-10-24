#pragma once
#include "battle.hpp"
#include <array>
namespace pokedex { namespace adventure {

constexpr uint8_t kPracticeBalls = 2;

enum class PracticeStage : uint8_t { Idle, Battle, ThrowReady, Throwing, Result };

struct PracticeThrowView {
  bool active = false;
  uint8_t attemptIndex = 0;
  uint32_t targetMs = 0;
};

struct PracticeOutcome {
  bool valid = false;
  uint8_t attemptIndex = 0;
  Timing timing = Timing::Miss;
  uint8_t probability = 0;
  uint8_t roll = 0;
  bool captured = false;
  uint32_t at = 0;
};

// 捕获练习只复用真实对战的节拍、投球时机与概率规则。
// 它不接触 Save、球库存或图鉴：成功只在本对象内结算。
class CapturePractice {
 public:
  void begin(uint32_t nowMs, uint32_t seed);
  void begin(uint16_t species, uint32_t nowMs, uint32_t seed);
  void reset();
  PracticeStage stage() const { return stage_; }
  bool active() const {
    return stage_ != PracticeStage::Idle && stage_ != PracticeStage::Result;
  }
  uint16_t species() const { return encounter_.species; }
  uint8_t usedBalls() const { return usedBalls_; }
  Result result() const { return result_; }
  const BattleView& battleView() const { return battle_.view(); }
  const PracticeThrowView& throwView() const { return throw_; }
  const std::array<ThrowAttempt, kPracticeBalls>& attempts() const { return encounter_.attempts; }
  PracticeOutcome tick(uint32_t nowMs);
  bool press(Action, uint32_t nowMs);
  bool requestPause();
  bool resume(uint32_t nowMs);
  bool beginThrow(uint32_t nowMs);
  PracticeOutcome pressThrow(uint32_t nowMs);

 private:
  PracticeOutcome resolve(uint32_t pressMs);
  void settleBattle();
  Battle battle_;
  Encounter encounter_;
  PracticeThrowView throw_;
  PracticeStage stage_ = PracticeStage::Idle;
  Result result_ = Result::None;
  uint8_t usedBalls_ = 0;
};

} }
