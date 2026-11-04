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
  d.select(MenuItem::Records);
  d.until([&] { return d.screen().page == Page::Records; }, 4000, "SDL input opens records");
  d.frames();
  d.check(capture_badge_test::badge(count, 134, 170), "records first-capture tile shows " + std::to_string(count) + " captured species");
  d.shot(name + "-records");
  d.back();
  d.back();
  d.until([&] { return d.screen().page == Page::Home; }, 4000, "SDL input returns home");
}
} }
