#pragma once
#include "battle.hpp"
#include "menu_layout.hpp"

namespace pokedex { namespace ui {
constexpr uint32_t kBattleEffectMs = 450;
inline bool battleEffectActive(const adventure::BattleView& battle, uint32_t now) {
  return !battle.paused && (battle.consumed || battle.cueIndex > 0) &&
         now - battle.settledMs < kBattleEffectMs;
}
inline adventure::Action battleEffectAction(const adventure::BattleView& battle) {
  return battle.settledCue;
}
constexpr int kPartnerX = 155, kPartnerY = 232, kEnemyX = 311, kEnemyY = 226;
// The cue circles the actor whose key it asks for: attack (right key) on the enemy, dodge (left key) on the partner.
constexpr int kCueTargetRadius = 48, kCueStartRadius = 92;
inline int cueRadius(int32_t remaining) {
  const int32_t lead = int32_t(adventure::kCueLeadMs);
  const int32_t left = remaining < 0 ? 0 : remaining > lead ? lead : remaining;
  return kCueTargetRadius + int((kCueStartRadius - kCueTargetRadius) * left / lead);
}
// A neutral next-beat dot slides left at 0.07 px/ms; no direction or judgment target.
constexpr int kLaneY = 357, kLaneHalfWidth = 165;
constexpr int laneOffset(int32_t remaining) { return int(remaining * 7 / 100); }
inline bool laneVisible(int32_t remaining) {
  const int offset = laneOffset(remaining);
  return offset >= -kLaneHalfWidth && offset <= kLaneHalfWidth;
}
constexpr int kCountdownY = 401;
constexpr menu_layout::Rect kPlayerPlate = {76, 96, 130, 38};
constexpr menu_layout::Rect kEnemyPlate = {260, 96, 130, 38};
constexpr int kPlateInset = 16;
constexpr int kHpSegmentY = 121;
constexpr int kHpGap = 4;
constexpr int hpSegmentWidth(unsigned total) {
  return (kPlayerPlate.width - 2 * kPlateInset - int(total - 1) * kHpGap) / int(total);
}
constexpr int hpSegmentStart(unsigned index, unsigned total) {
  return int(index) * (hpSegmentWidth(total) + kHpGap);
}
constexpr int hpTrackWidth(unsigned total) { return hpSegmentStart(total, total) - kHpGap; }
constexpr int hpSegmentX(bool enemy, unsigned index, unsigned total) {
  return (enemy ? kEnemyPlate.x + kEnemyPlate.width - kPlateInset - hpTrackWidth(total)
                : kPlayerPlate.x + kPlateInset) + hpSegmentStart(index, total) + hpSegmentWidth(total) / 2;
}
} }
