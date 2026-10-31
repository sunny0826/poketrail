#pragma once
#include "driver.hpp"
#include "../support/capture_badge.hpp"

namespace simulator { namespace e2e {
inline void checkCaptureBadge(Driver& d, unsigned count, const std::string& name) {
  d.frames();
  d.check(d.screen().page == Page::Home, "capture badge check starts on home");
  d.check(capture_badge_test::badge(count, 318, 81), "home shows " + std::to_string(count) + " captured species");
  d.shot(name + "-home");
  d.homeMenu();
  d.select(4);
  d.until([&] { return d.screen().page == Page::Resources; }, 4000, "SDL input opens resources");
  d.check(capture_badge_test::badge(count, 312, 110), "resources shows " + std::to_string(count) + " captured species");
  d.shot(name + "-resources");
  d.back();
  d.back();
  d.until([&] { return d.screen().page == Page::Home; }, 4000, "SDL input returns home");
}
} }
