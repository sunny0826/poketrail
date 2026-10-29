#pragma once
#include "adventure_model.hpp"
namespace pokedex { namespace adventure {
constexpr uint32_t kBeatIntervalMs = 1500;
constexpr uint32_t kFrenzyBeatIntervalMs = 1100;
constexpr uint32_t kNormalWindowMs = 180;
constexpr uint32_t kPerfectWindowMs = 70;
constexpr uint32_t kFrenzyNormalWindowMs = 140;
constexpr uint32_t kFrenzyPerfectWindowMs = 55;
constexpr uint32_t kCueLeadMs = 700;
constexpr uint32_t kFeintFlipMs = 350;
constexpr uint32_t kChordGraceMs = 80;
constexpr uint8_t kPlayerHp = 3;
constexpr uint8_t kEnemyHp = 5;
constexpr uint8_t kMaxCues = 16;
constexpr uint8_t kSpecialEnergy = 3;
constexpr uint8_t kSpecialDamage = 3;
constexpr uint8_t kFeintPercent = 25;
constexpr uint8_t kRareFeintPercent = 40;
struct BattleView {
  bool active = false;
  bool paused = false;
  bool pauseRequested = false;
  bool consumed = false;
  bool feint = false;
  bool frenzy = false;
  bool special = false;
  uint8_t playerHp = kPlayerHp;
  uint8_t enemyHp = kEnemyHp;
  uint8_t energy = 0;
  uint8_t cueIndex = 0;
  uint32_t targetMs = 0;
  uint32_t settledMs = 0;
  Action expected = Action::Attack;
  Action settledAction = Action::Attack;   // key(s) the player used on the settled cue
  Action settledCue = Action::Attack;      // what the settled cue required; drives its effect
  Timing lastTiming = Timing::Miss;
  Result result = Result::None;
};
class Battle {
 public:
  void begin(uint32_t nowMs, uint32_t seed = 0, bool rare = false);
  // A second key within kChordGraceMs of the first upgrades the cue to a chord.
  bool press(Action, uint32_t nowMs);
  bool tick(uint32_t nowMs);
  bool requestPause();
  bool resume(uint32_t nowMs);
  const BattleView& view() const { return view_; }
 private:
  void settle(Action, Timing, uint32_t nowMs);
  bool upgrade(Action, uint32_t nowMs);
  void advance();
  void plan();
  uint32_t interval() const;
  BattleView view_;
  uint8_t beforePlayerHp_ = kPlayerHp, beforeEnemyHp_ = kEnemyHp, beforeEnergy_ = 0;
  uint32_t seed_ = 0;
  uint32_t pressMs_ = 0;
  uint8_t feintPercent_ = kFeintPercent;
  uint8_t history_ = 0;
};
bool timeAfter(uint32_t a, uint32_t b);
Timing battleTiming(uint32_t pressMs, uint32_t targetMs, bool frenzy = false);
Timing throwTiming(uint32_t pressMs, uint32_t targetMs);
// The action the cue ring shows at nowMs; feints flip to the judged action late.
Action shownAction(const BattleView&, uint32_t nowMs);
} }
