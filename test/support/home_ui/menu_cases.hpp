#pragma once
#include "pixel_checks.hpp"
#include <cstring>

// Main menu gestures, paging, return row and route/partner selection.
namespace {
void openingGestures() {
  for (unsigned gesture = 0; gesture < 4; ++gesture) {
    MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    const bool left = gesture != 1, right = gesture == 1 || gesture == 2;
    buttons(left, right); frame(runtime);
    check(runtime.screen().page == Page::Menu && runtime.screen().selected == 0,
          "left, right and dual presses open menu immediately at zero");
    idle(runtime, 900); buttons(true, true); frame(runtime); idle(runtime, 120);
    buttons(false, true); frame(runtime); buttons(true, true); frame(runtime);
    buttons(true, false); frame(runtime); idle(runtime, 100);
    check(runtime.screen().page == Page::Menu && runtime.screen().selected == 0,
          "long hold, overlap and partial releases consume the complete opening gesture");
    settle(runtime); edge(runtime, false, true);
    check(runtime.screen().page == Page::Menu && runtime.screen().selected == 1,
          "a new released single key navigates after full release");
    select(runtime, 9); settle(runtime); home(runtime);
  }
}
void ninthItem() {
  MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); check(menuCount(runtime.screen(), runtime.engine()) == 10, "menu has ten entries");
  edge(runtime, true, false);
  check(runtime.screen().selected == 9, "left from first item wraps to return on page four");
  const auto bytes = store.bytes;
  screenshot(runtime, "home-menu-fourth-page");
  tap(runtime, 233, 150); frame(runtime);
  check(runtime.screen().page == Page::Home && store.bytes == bytes,
        "fourth-page return row leaves the menu without writing a save");
  openMenu(runtime);
  tap(runtime, 298, 430); frame(runtime); check(runtime.screen().selected == 3, "page control reaches second page");
  tap(runtime, 298, 430); frame(runtime); check(runtime.screen().selected == 6, "page control reaches third page");
  tap(runtime, 233, 150); settle(runtime);
  check(runtime.dimmed() && runtime.screen().page == Page::Menu && runtime.screen().selected == 6,
        "third-page sleep row dims without leaving the menu");
  tap(runtime, 233, 150); settle(runtime);
  check(!runtime.dimmed() && runtime.screen().page == Page::Menu,
        "wake touch cannot activate a menu row");
  tap(runtime, 168, 430); frame(runtime);
  check(runtime.screen().selected == 3, "previous page control returns to page two");
  tap(runtime, 298, 430); tap(runtime, 298, 430); tap(runtime, 298, 430); frame(runtime);
  check(runtime.screen().selected == 0, "page control wraps from the fourth page to the first");
  tap(runtime, 168, 430); frame(runtime);
  check(runtime.screen().selected == 9, "previous page control wraps to the last page");
  select(runtime, 9); settle(runtime); home(runtime);
  // A partial page still ignores its blank row: Partners shows one owned partner
  // plus return, so row three must neither activate nor write.
  openMenu(runtime); select(runtime, 2);
  const auto partnerBytes = store.bytes;
  const auto partnerSelection = runtime.screen().selected;
  tap(runtime, 233, 374); frame(runtime);
  check(runtime.screen().page == Page::Partners && runtime.screen().selected == partnerSelection &&
        store.bytes == partnerBytes, "blank list row neither activates nor changes selection or save");
  select(runtime, 1);
  select(runtime, 9);
  settle(runtime); home(runtime);
}
void routesAndPartner() {
  MemoryStore store; legacyOwned(store, {1, 4, 7}); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  uint16_t colors[3] = {};
  for (unsigned route = 0; route < 3; ++route) {
    openMenu(runtime); select(runtime, 1);
    if (route == 0) screenshot(runtime, "routes-menu");
    select(runtime, route); settle(runtime); home(runtime);
    check(runtime.engine().view().route == Route(route), "real route menu updates the displayed route");
    check(glyph(routeName(Route(route)), 81, theme::routeColor(Route(route)), 107, true), "home labels the actual current scene");
    colors[route] = M5.Display.readPixel(233, 110);
    char name[40]; std::snprintf(name, sizeof(name), "home-scene-%u", route); screenshot(runtime, name);
    Save loaded; check(store.load(loaded).status == LoadStatus::Loaded && loaded.route == Route(route),
                       "route selection survives production codec reload");
  }
  check(colors[0] != colors[1] && colors[1] != colors[2] && colors[0] != colors[2], "three scenes have distinct visible backgrounds");
  openMenu(runtime); select(runtime, 2); select(runtime, 1);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 4, "partner list opens Charmander detail");
  select(runtime, 0); settle(runtime); check(runtime.engine().view().partner == 4, "carry action selects Charmander");
  select(runtime, menuCount(runtime.screen(), runtime.engine()) - 1); select(runtime, 3); select(runtime, 9);
  settle(runtime); home(runtime);
  screenshot(runtime, "home-partner-switched");
  check(textLine(speciesName(4), 52, theme::kText, 240), "home name follows the current carried partner");
  Save loaded; check(store.load(loaded).status == LoadStatus::Loaded && loaded.partner == 4, "carried partner is durable");
}
}
