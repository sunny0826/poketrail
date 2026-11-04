#include "driver.hpp"

namespace simulator { namespace e2e {
void aimBeforeThrow(Driver& d) {
  d.frames();
  d.check(d.screen().page == Page::Throw && d.save().encounter.usedBalls == 0,
          "throw fixture opens with two attempts available");
  d.advance(3000);
  d.check(d.save().encounter.usedBalls == 0, "waiting does not spend an attempt");
  d.tap(233, 238);
  d.check(d.save().encounter.usedBalls == 0, "a tap does not spend an attempt");
  d.tap(233, 40);
  d.check(d.screen().page == Page::Throw && d.save().encounter.usedBalls == 0,
          "the top edge does not exit or spend an attempt");
  d.key(SDL_SCANCODE_LEFT, true);
  d.key(SDL_SCANCODE_RIGHT, true);
  d.key(SDL_SCANCODE_RIGHT, false);
  d.check(!d.screen().charging && d.save().encounter.usedBalls == 0,
          "a short right press cancels even while left remains held");
  d.mouse(233, 238, true);
  d.advance(80);
  d.mouse(233, 238, false);
  d.check(!d.screen().charging && d.save().encounter.usedBalls == 0,
          "a short touch cancels even while left remains held");
  d.key(SDL_SCANCODE_LEFT, false);
  d.frames();
  d.mouse(233, kHandY, true);
  d.check(d.screen().charging && d.save().encounter.usedBalls == 0,
          "holding touch charges without spending an attempt");
  d.advance(1000);
  d.shot("throw-touch-held");
  d.mouse(233, kHandY, false);
  d.finishCapture();
  waitSettled(d, "the aimed throw settles");
  d.check(d.save().encounter.usedBalls == 1, "the released throw spends exactly one attempt");
  d.check(d.save().encounter.attempts[0].timing == Timing::Miss, "the early release scores a miss");
  d.check(d.screen().page == Page::Throw, "the first miss keeps the encounter throw ready");
  d.shot("throw-aimed-miss");
}
} }
