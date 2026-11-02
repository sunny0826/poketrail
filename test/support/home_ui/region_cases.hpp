#pragma once
#include "pixel_checks.hpp"

namespace {
void selectEveryRegion(MemoryStore& store, Runtime& runtime) {
  check(runtime.engine().view().route == Route::Kanto, "upgraded save starts in Kanto");
  uint16_t colors[kRegionCount] = {};
  for (unsigned index = 0; index < kRegionCount; ++index) {
    const Route region = regionAt(uint8_t(index));
    openMenu(runtime); select(runtime, 1);
    if (index == 0) screenshot(runtime, "routes-menu");
    if (index == 3) {
      swipe(runtime, 300, 300, 160, 300); frame(runtime);
      check(runtime.screen().page == Page::Routes && runtime.screen().selected == 3,
            "region menu pages to Sinnoh, Unova and back");
      screenshot(runtime, "routes-menu-second-page");
      check(textLine("选择地区", 52, theme::kText, 240) &&
            glyph("只影响下一场遭遇", 80, theme::routeColor(Route::Sinnoh)),
            "region page title follows the highlighted region");
    }
    select(runtime, index);
    settle(runtime); home(runtime);
    check(runtime.engine().view().route == region, "real region menu updates the displayed region");
    check(glyph(routeName(region), 81, theme::routeColor(region), 107, true), "home labels the actual current scene");
    colors[index] = M5.Display.readPixel(233, 110);
    char name[40]; std::snprintf(name, sizeof(name), "home-scene-%u", index); screenshot(runtime, name);
    Save loaded; check(store.load(loaded).status == LoadStatus::Loaded && loaded.route == region,
                       "region selection survives production codec reload");
  }
  for (unsigned a = 0; a < kRegionCount; ++a) for (unsigned b = a + 1; b < kRegionCount; ++b)
    check(colors[a] != colors[b] && theme::routeColor(regionAt(uint8_t(a))) != theme::routeColor(regionAt(uint8_t(b))),
          "five regions have distinct visible backgrounds and accents");
}
}  // namespace
