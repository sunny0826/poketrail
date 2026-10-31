#include "driver.hpp"

namespace simulator {
namespace e2e {
void aimBeforeThrow(Driver& d) {
  d.frames();
  d.check(d.screen().page == Page::Throw, "throw fixture opens throw page");
  d.check(d.save().encounter.usedBalls == 0, "fixture has no attempts");
  d.advance(3000);
  d.check(!d.runtime().engine().throwView().active, "waiting does not open the throw window");
  d.check(d.save().encounter.usedBalls == 0, "waiting does not spend an attempt");
  d.tap(233, 238);
  d.check(!d.runtime().engine().throwView().active, "a tap does not open the throw window");
  d.check(d.save().encounter.usedBalls == 0, "a tap does not spend an attempt");
  d.tap(233, 40);
  d.check(!d.runtime().engine().throwView().active, "the top edge does not open the throw window");
  d.check(d.save().encounter.usedBalls == 0, "the top edge does not spend an attempt");
  d.shot("throw-aiming");
  d.key(SDL_SCANCODE_LEFT, true);
  d.key(SDL_SCANCODE_RIGHT, true);
  d.check(d.runtime().engine().throwView().active, "right starts charging while left is held");
  d.key(SDL_SCANCODE_RIGHT, false);
  d.check(!d.runtime().engine().throwView().active && d.save().encounter.usedBalls == 0,
          "releasing a short right press cancels even while left remains held");
  d.mouse(233, 238, true);
  d.check(d.runtime().engine().throwView().active, "touch starts charging while left is held");
  d.advance(80);
  d.check(d.runtime().engine().throwView().active, "touch remains charging while both touch and left are held");
  d.mouse(233, 238, false);
  d.check(!d.runtime().engine().throwView().active && d.save().encounter.usedBalls == 0,
          "short touch release cancels even while left remains held");
  d.key(SDL_SCANCODE_LEFT, false);
  d.key(SDL_SCANCODE_RIGHT, true);
  d.check(d.runtime().engine().throwView().active, "holding right opens the throw window");
  d.check(d.save().encounter.usedBalls == 0, "the hold does not spend an attempt yet");
  // 超过短按阈值但远早于目标窗口，必须记为 Miss。
  d.advance(600);
  d.key(SDL_SCANCODE_RIGHT, false, 1);
  d.check(!d.runtime().engine().throwView().active, "the release settles the throw");
  d.finishCapture();
  d.until([&] {
    const Page p = d.screen().page;
    return p == Page::Result || p == Page::Error || p == Page::Throw;
  }, 20000, "the aimed throw settles");
  d.check(d.save().encounter.usedBalls == 1, "the released throw spends exactly one attempt");
  d.check(d.save().encounter.attempts[0].used, "the attempt is recorded");
  d.check(d.save().encounter.attempts[0].timing == Timing::Miss, "the early release scores a miss");
  d.check(d.screen().page == Page::Throw, "the first miss keeps the encounter throw ready");
  d.shot("throw-aimed-miss");
}
}
}
