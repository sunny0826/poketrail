#include "game.hpp"

#include <cstdlib>

namespace pokedex {
namespace viewer {

Input releaseInput(int32_t start_x, int32_t start_y, int32_t end_x,
                   int32_t end_y, bool clicked) {
  const int32_t dx = end_x - start_x;
  const int32_t dy = end_y - start_y;
  if (std::abs(dx) >= kSwipeDistance && std::abs(dx) > std::abs(dy))
    return dx < 0 ? Input::Next : Input::Previous;
  if (clicked && std::abs(dx) <= 12 && std::abs(dy) <= 12 &&
      std::abs(start_x - kCenter) <= 140 && std::abs(start_y - kCenter) <= 140)
    return Input::React;
  return Input::None;
}

bool applyInput(State& state, Input input, size_t count, uint32_t now_ms) {
  if (input == Input::None || !count) return false;
  if (input == Input::React) {
    state.reacting = true;
    state.reaction_started_ms = now_ms;
    state.jump = 0;
    return true;
  }
  if (input == Input::Next)
    state.sprite_index = (state.sprite_index + 1) % count;
  else
    state.sprite_index = (state.sprite_index + count - 1) % count;
  state.frame_index = 0;
  state.animation_started_ms = now_ms;
  state.reacting = false;
  state.jump = 0;
  return true;
}

bool update(State& state, const Sprite& sprite, uint32_t now_ms) {
  const uint16_t old_frame = state.frame_index;
  const bool was_reacting = state.reacting;
  const int16_t old_jump = state.jump;
  if (sprite.duration_ms && sprite.frame_count) {
    uint32_t position = (now_ms - state.animation_started_ms) % sprite.duration_ms;
    uint16_t frame = 0;
    while (frame + 1 < sprite.frame_count && position >= sprite.frames[frame].duration_ms) {
      position -= sprite.frames[frame].duration_ms;
      ++frame;
    }
    state.frame_index = frame;
  }
  if (state.reacting) {
    const uint32_t elapsed = now_ms - state.reaction_started_ms;
    state.reacting = elapsed < kReactionMs;
    // 一次轻跳，后半段自然落回中心；不重启原始动作。
    const int32_t t = elapsed < 480 ? static_cast<int32_t>(elapsed) : 480;
    state.jump = state.reacting ? 4 * 14 * t * (480 - t) / (480 * 480) : 0;
  }
  return old_frame != state.frame_index || was_reacting != state.reacting ||
         old_jump != state.jump;
}

}  // namespace viewer
}  // namespace pokedex
