#pragma once
#include "pixel_checks.hpp"

namespace {
void homeTouch() {
  MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  home(runtime); const auto bytes = store.bytes; const unsigned commits = store.commits;
  for (int y : {30, 228, 276, 324, 372, 422}) for (int x : {90, 233, 375}) {
    tap(runtime, x, y);
    check(runtime.screen().page == Page::Menu && runtime.engine().view().partner == 1 &&
          runtime.engine().view().route == Route::Forest,
          "every home touch opens the menu without changing partner or route");
    tap(runtime, 233, 52); home(runtime);
  }
  auto& touch = M5.Touch.detail;
  touch.base_x = touch.x = 233; touch.base_y = touch.y = 233; touch.pressed = true;
  frame(runtime); buttons(true, false); frame(runtime); buttons(true, true); frame(runtime);
  idle(runtime, 240);
  check(runtime.screen().page == Page::Menu, "mixed home touch and key hold cannot leak into menu actions");
  touch.pressed = false; touch.released = touch.clicked = true; frame(runtime); touch = {};
  buttons(false, true); frame(runtime); settle(runtime);
  check(runtime.screen().page == Page::Menu && store.bytes == bytes && store.commits == commits,
        "home menu gestures perform no save transaction");
  tap(runtime, 233, 52); home(runtime);
  openMenu(runtime, true); select(runtime, 9); settle(runtime); home(runtime);
}
}
