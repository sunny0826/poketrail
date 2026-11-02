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
