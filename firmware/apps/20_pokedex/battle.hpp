#pragma once
#include "adventure_model.hpp"
namespace pokedex { namespace adventure {
constexpr uint32_t kBeatIntervalMs = 1500;
constexpr uint32_t kNormalWindowMs = 180;
constexpr uint32_t kPerfectWindowMs = 70;
enum class TelegraphPattern : uint8_t {
  Alternate = 0,  // Attack, Dodge, Attack, Dodge...
  AlternateDodgeFirst = 1,
  AttackOnly = 2,
  DodgeOnly = 3
};
// Parameterizes one battle without any species branch. Max HP bounds the bars,
// start HP lets an activity stage begin partly damaged, and the guard absorbs
// that many missed dodges before HP is lost.
struct BattleSpec {
  uint8_t playerHp = 3;
  uint8_t enemyHp = 3;
  uint8_t playerStartHp = 3;
  uint8_t enemyStartHp = 3;
  uint8_t cueLimit = 12;
  uint32_t beatMs = kBeatIntervalMs;
  uint32_t normalWindowMs = kNormalWindowMs;
  uint32_t perfectWindowMs = kPerfectWindowMs;
  TelegraphPattern telegraph = TelegraphPattern::Alternate;
  uint8_t missedDodgeGuard = 0;
  Action expected(uint8_t cueIndex) const;
};
struct BattleView {
  bool active = false;
  bool paused = false;
  bool pauseRequested = false;
  bool consumed = false;
  uint8_t playerHp = 3;
  uint8_t enemyHp = 3;
  uint8_t playerMaxHp = 3;
  uint8_t enemyMaxHp = 3;
  uint8_t cueIndex = 0;
  uint32_t targetMs = 0;
  uint32_t settledMs = 0;
  Action expected = Action::Attack;
  Timing lastTiming = Timing::Miss;
  // The action actually settled by the last beat. The next cue cannot supply it:
  // AttackOnly/DodgeOnly would be inverted. Presentation reads this directly.
  Action lastAction = Action::Attack;
  bool lastActionValid = false;
  Result result = Result::None;
  uint32_t beatMs = kBeatIntervalMs;
  uint32_t normalWindowMs = kNormalWindowMs;
  uint32_t perfectWindowMs = kPerfectWindowMs;
  uint8_t guard = 0;
  uint8_t perfectCount = 0;
};
class Battle {
 public:
  void begin(uint32_t nowMs, uint32_t seed = 0);
  void begin(uint32_t nowMs, uint32_t seed, const BattleSpec& spec);
  bool press(Action, uint32_t nowMs);
  bool tick(uint32_t nowMs);
  bool requestPause();
  bool resume(uint32_t nowMs);
  const BattleView& view() const { return view_; }
 private:
  void settle(Timing, uint32_t nowMs);
  void advance();
  BattleSpec spec_;
  BattleView view_;
};
bool timeAfter(uint32_t a, uint32_t b);
Timing battleTiming(uint32_t pressMs, uint32_t targetMs,
                    uint32_t perfectWindowMs = kPerfectWindowMs,
                    uint32_t normalWindowMs = kNormalWindowMs);
Timing throwTiming(uint32_t pressMs, uint32_t targetMs);
} }
