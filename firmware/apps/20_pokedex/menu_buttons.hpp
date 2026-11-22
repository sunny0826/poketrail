#pragma once
#include <stdint.h>
namespace pokedex { namespace ui {
enum class MenuAction : uint8_t { None, Previous, Next, Confirm };
class MenuButtons {
 public:
  static constexpr uint32_t kRepeatDelayMs = 350;
  static constexpr uint32_t kRepeatIntervalMs = 120;
  static constexpr uint32_t kRepeatFastDelayMs = 1200;
  static constexpr uint32_t kRepeatFastIntervalMs = 65;

  void reset() { state_ = State::Idle; pressedAt_ = 0; lastRepeatAt_ = 0; }
  void consume() { state_ = State::Consumed; }
  MenuAction update(bool left, bool right) { return update(left, right, 0, false); }
  MenuAction update(bool left, bool right, uint32_t nowMs, bool repeatAllowed) {
    if (state_ == State::Consumed) {
      if (!left && !right) reset();
      return MenuAction::None;
    }
    if (state_ == State::RepeatingLeft || state_ == State::RepeatingRight) {
      const bool activeDown = state_ == State::RepeatingLeft ? left : right;
      const bool otherDown = state_ == State::RepeatingLeft ? right : left;
      if (!activeDown || otherDown || !repeatAllowed) {
        state_ = (left || right) ? State::Consumed : State::Idle;
        return MenuAction::None;
      }
      const uint32_t heldMs = nowMs - pressedAt_;
      const uint32_t interval =
          heldMs >= kRepeatFastDelayMs ? kRepeatFastIntervalMs : kRepeatIntervalMs;
      if (nowMs - lastRepeatAt_ >= interval) {
        lastRepeatAt_ = nowMs;
        return state_ == State::RepeatingLeft ? MenuAction::Previous : MenuAction::Next;
      }
      return MenuAction::None;
    }
    if (left && right) {
      state_ = State::Consumed;
      return MenuAction::Confirm;
    }
    if ((state_ == State::Left && left && !right) ||
        (state_ == State::Right && right && !left)) {
      if (repeatAllowed && nowMs - pressedAt_ >= kRepeatDelayMs) {
        lastRepeatAt_ = nowMs;
        state_ = state_ == State::Left ? State::RepeatingLeft : State::RepeatingRight;
        return state_ == State::RepeatingLeft ? MenuAction::Previous : MenuAction::Next;
      }
      return MenuAction::None;
    }
    const State next = left ? State::Left : right ? State::Right : State::Idle;
    MenuAction action = MenuAction::None;
    if (state_ == State::Left && !left) action = MenuAction::Previous;
    if (state_ == State::Right && !right) action = MenuAction::Next;
    state_ = next;
    if (next == State::Left || next == State::Right) pressedAt_ = nowMs;
    return action;
  }
 private:
  enum class State : uint8_t { Idle, Left, Right, RepeatingLeft, RepeatingRight, Consumed };
  State state_ = State::Idle;
  uint32_t pressedAt_ = 0;
  uint32_t lastRepeatAt_ = 0;
};
} }
