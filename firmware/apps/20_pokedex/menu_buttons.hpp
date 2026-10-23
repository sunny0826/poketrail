#pragma once
#include <stdint.h>
namespace pokedex { namespace ui {
enum class MenuAction : uint8_t { None, Previous, Next, Confirm };
class MenuButtons {
 public:
  void reset() { state_ = State::Idle; }
  MenuAction update(bool left, bool right) {
    if (state_ == State::Consumed) {
      if (!left && !right) reset();
      return MenuAction::None;
    }
    if (left && right) {
      state_ = State::Consumed;
      return MenuAction::Confirm;
    }
    const State next = left ? State::Left : right ? State::Right : State::Idle;
    MenuAction action = MenuAction::None;
    if (state_ == State::Left && !left) action = MenuAction::Previous;
    if (state_ == State::Right && !right) action = MenuAction::Next;
    state_ = next;
    return action;
  }
 private:
  enum class State : uint8_t { Idle, Left, Right, Consumed };
  State state_ = State::Idle;
};
} }
