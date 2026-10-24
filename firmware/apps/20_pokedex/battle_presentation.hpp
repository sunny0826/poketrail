#pragma once
#include "battle.hpp"

namespace pokedex { namespace ui {
constexpr uint32_t kBattleEffectMs = 450;
inline bool battleEffectActive(const adventure::BattleView& battle, uint32_t now) {
  return !battle.paused && (battle.consumed || battle.cueIndex > 0) &&
         now - battle.settledMs < kBattleEffectMs;
}
inline adventure::Action battleEffectAction(const adventure::BattleView& battle) {
  if (battle.consumed) return battle.expected;
  return battle.expected == adventure::Action::Attack ? adventure::Action::Dodge : adventure::Action::Attack;
}
} }
