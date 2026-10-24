#pragma once
#include "adventure_model.hpp"
namespace pokedex { namespace adventure {
constexpr uint32_t kBeatIntervalMs = 1500;
constexpr uint32_t kNormalWindowMs = 180;
constexpr uint32_t kPerfectWindowMs = 70;
struct BattleView {
  bool active = false;
  bool paused = false;
  bool pauseRequested = false;
  bool consumed = false;
  uint8_t playerHp = 3;
  uint8_t enemyHp = 3;
  uint8_t cueIndex = 0;
  uint32_t targetMs = 0;
  uint32_t settledMs = 0;
  Action expected = Action::Attack;
  Timing lastTiming = Timing::Miss;
  Result result = Result::None;
};
class Battle {
 public:
  void begin(uint32_t nowMs, uint32_t seed = 0);
  bool press(Action, uint32_t nowMs);
  bool tick(uint32_t nowMs);
  bool requestPause();
  bool resume(uint32_t nowMs);
  const BattleView& view() const { return view_; }
 private:
  void settle(Timing, uint32_t nowMs);
  void advance();
  BattleView view_;
};
bool timeAfter(uint32_t a, uint32_t b);
Timing battleTiming(uint32_t pressMs, uint32_t targetMs);
Timing throwTiming(uint32_t pressMs, uint32_t targetMs);
} }
