#pragma once
#include <utility/Button_Class.hpp>
#include "runtime.hpp"
namespace pokedex { namespace ui {
class ButtonInput {
 public:
  void sample(Input& input, const m5::Button_Class& left, const m5::Button_Class& right) {
    input.left = left.wasPressed(); input.right = right.wasPressed();
    input.leftDown = left.isPressed(); input.rightDown = right.isPressed();
    input.held = input.held || input.leftDown || input.rightDown;
    const bool both = input.leftDown && input.rightDown;
    if (!both || !bothDown_) { bothSince_ = input.at; pauseSent_ = false; }
    bothDown_ = both;
    if (both && !pauseSent_ && input.at - bothSince_ >= 800) {
      input.pause = true; pauseSent_ = true;
    }
  }
 private:
  uint32_t bothSince_ = 0;
  bool pauseSent_ = false, bothDown_ = false;
};
} }
