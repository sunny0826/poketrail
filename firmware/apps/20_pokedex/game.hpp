#pragma once

#include "sprite.hpp"

namespace pokedex {
namespace viewer {

constexpr int32_t kCenter = 233;
constexpr uint32_t kReactionMs = 800;
constexpr int32_t kSwipeDistance = 45;

enum class Input : uint8_t { None, React, Previous, Next };

struct State {
  uint16_t sprite_index = 0;
  uint16_t frame_index = 0;
  uint32_t animation_started_ms = 0;
  uint32_t reaction_started_ms = 0;
  bool reacting = false;
  int16_t jump = 0;
};

Input releaseInput(int32_t start_x, int32_t start_y, int32_t end_x,
                   int32_t end_y, bool clicked);
bool applyInput(State& state, Input input, size_t count, uint32_t now_ms);
bool update(State& state, const Sprite& sprite, uint32_t now_ms);

}  // namespace viewer
}  // namespace pokedex
