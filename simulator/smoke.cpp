#include "app.hpp"
#include "viewer_screen.hpp"
#include <cstdio>
#include <stdexcept>

namespace simulator {
namespace {
void check(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
  std::printf("PASS %s\n", message);
}
void frames(App& app, unsigned count = 4) {
  for (unsigned i = 0; i < count; ++i)
    if (!app.step()) throw std::runtime_error("desktop event loop closed unexpectedly");
}
void event(App& app, SDL_Event& event) {
  if (SDL_PushEvent(&event) != 1) throw std::runtime_error("SDL rejected input event");
  frames(app);
}
void key(App& app, SDL_Scancode code, bool down) {
  SDL_Event e = {}; e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  e.key.keysym.scancode = code; event(app, e);
}
void press(App& app, SDL_Scancode code) { key(app, code, true); key(app, code, false); }
void mouse(App& app, int x, int y, bool down) {
  SDL_Event e = {}; e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  e.button.button = SDL_BUTTON_LEFT; e.button.x = x + kScreenX; e.button.y = y + kScreenY;
  event(app, e);
}
void tap(App& app, int x, int y) { mouse(app, x, y, true); mouse(app, x, y, false); }
void snapshot(App& app, const char* name) {
  const std::string path = app.screenshot(name);
  SDL_Surface* bmp = SDL_LoadBMP(path.c_str());
  if (!bmp) throw std::runtime_error(SDL_GetError());
  SDL_Surface* rgb = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGB24, 0);
  SDL_FreeSurface(bmp);
  if (!rgb) throw std::runtime_error(SDL_GetError());
  bool matches = true;
  std::vector<uint8_t> expected(kScreenSize * 3);
  for (int y = 0; y < kScreenSize; ++y) {
    M5.Display.readRectRGB(0, y, kScreenSize, 1, expected.data());
    const auto* row = static_cast<const uint8_t*>(rgb->pixels) + (y + kScreenY) * rgb->pitch;
    for (int x = 0; x < kScreenSize; ++x) {
      if ((x - 233) * (x - 233) + (y - 233) * (y - 233) > 233 * 233) continue;
      for (int c = 0; c < 3; ++c)
        if (row[(x + kScreenX) * 3 + c] != expected[x * 3 + c]) matches = false;
    }
  }
  SDL_FreeSurface(rgb);
  check(matches, "desktop screenshot preserves every circular M5GFX pixel and color");
}
void home(App& app) {
  using pokedex::ui::Page;
  check(app.runtime().screen().page == Page::Home, "home is visible");
}
}
void smoke(App& app, bool reload) {
  using pokedex::ui::Page;
  using pokedex::adventure::Phase;
  std::printf("SDL video driver: %s\n", SDL_GetCurrentVideoDriver());
  frames(app);
  if (reload) {
    home(app);
    check(app.runtime().engine().view().starter == 1, "starter survives process restart");
    check(app.runtime().engine().view().totalSteps >= 500, "walking progress survives process restart");
    check(app.runtime().engine().view().phase == Phase::EncounterReady, "encounter survives process restart");
    app.screenshot("reload");
    check(app.close(), "reloaded session closes successfully");
    return;
  }
  check(app.runtime().screen().page == Page::DateWelcome, "new save opens date welcome");
  app.screenshot("first-boot");
  press(app, SDL_SCANCODE_RIGHT);
  press(app, SDL_SCANCODE_RETURN);
  check(app.runtime().screen().page == Page::Starter, "keyboard skips date through double-button confirmation");
  tap(app, 233, 238);
  home(app);
  check(app.runtime().engine().view().starter == 1, "mouse selects Bulbasaur");
  snapshot(app, "home");
  press(app, SDL_SCANCODE_RIGHT);
  check(app.runtime().screen().page == Page::Menu, "keyboard opens menu");
  mouse(app, 350, 300, true); mouse(app, 100, 300, false);
  check(app.runtime().screen().selected == 3, "mouse swipe reaches second menu page");
  tap(app, 233, 150);
  check(app.runtime().screen().page == Page::Viewer, "touch opens real Pokedex viewer");
  const auto species = pokedex::viewer_screen::species();
  press(app, SDL_SCANCODE_RIGHT);
  check(pokedex::viewer_screen::species() != species, "keyboard changes animated Pokedex species");
  app.screenshot("viewer");
  tap(app, 233, 35); home(app);
  press(app, SDL_SCANCODE_P);
  check(app.runtime().dimmed(), "power key sleeps display");
  tap(app, 233, 238);
  check(app.runtime().dimmed(), "touch cannot wake sleeping display");
  press(app, SDL_SCANCODE_P);
  check(!app.runtime().dimmed(), "power key wakes display");
  press(app, SDL_SCANCODE_W); press(app, SDL_SCANCODE_TAB);
  for (unsigned i = 0; i < 1100 && app.runtime().engine().view().totalSteps < 500; ++i)
    if (!app.step()) throw std::runtime_error("simulator stopped during walking");
  check(app.runtime().engine().view().totalSteps >= 500, "synthetic walking crosses real motion detector");
  check(app.runtime().engine().view().phase == Phase::EncounterReady, "walking creates a real encounter");
  press(app, SDL_SCANCODE_W); press(app, SDL_SCANCODE_TAB);
  if (app.runtime().dimmed()) press(app, SDL_SCANCODE_P);
  app.screenshot("walked");
  press(app, SDL_SCANCODE_RIGHT);
  tap(app, 233, 150);
  check(app.runtime().screen().page == Page::Encounter, "menu opens the saved encounter");
  app.screenshot("encounter");
  tap(app, 233, 238);
  check(app.runtime().screen().page == Page::Battle, "touch starts real rhythm battle");
  app.screenshot("battle");
  tap(app, 233, 35);
  for (unsigned i = 0; i < 200 && app.runtime().screen().page != Page::Pause; ++i) app.step();
  check(app.runtime().screen().page == Page::Pause, "touch pauses battle");
  key(app, SDL_SCANCODE_RIGHT, true);
  SDL_Event focus = {}; focus.type = SDL_WINDOWEVENT; focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
  focus.window.windowID = app.windowId();
  event(app, focus);
  check(!M5.button_a_down && !M5.button_b_down && !M5.Touch.detail.pressed,
        "losing focus releases held controls");
  tap(app, 233, 238);
  check(app.runtime().screen().page == Page::Battle, "paused battle resumes before close");
  SDL_Event quit = {}; quit.type = SDL_QUIT;
  check(SDL_PushEvent(&quit) == 1, "window close enters SDL queue");
  bool open = true;
  for (unsigned i = 0; i < 200 && open; ++i) open = app.step();
  check(!open, "closing an active battle pauses and checkpoints before exit");
  std::puts("PASS desktop simulator smoke");
}
}
