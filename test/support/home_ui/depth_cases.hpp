#pragma once
#include "pixel_checks.hpp"

namespace {
uint16_t expectedScenePixel(Route route, int x, int y, uint64_t visualSteps) {
  const unsigned index = uint8_t(route);
  const unsigned sourceX = (x - 5) / 2, sourceY = (y - 92) / 2;
  if (!visualSteps) return data::kRouteScenes[index].pixels[sourceY * data::kSceneWidth + sourceX];
  const auto& layers = data::kRouteSceneLayers[index];
  const unsigned top = layers.groundTop, height = data::kSceneHeight - top;
  uint16_t pixel = layers.far.pixels[sourceY * data::kSceneWidth + sourceX];
  if (sourceY < top) return pixel;
  const unsigned middleY = top + (sourceY - top + height - visualSteps % height) % height;
  const unsigned nearY = top + (sourceY - top + height - (visualSteps % height * 2) % height) % height;
  const uint16_t middle = layers.middle.pixels[middleY * data::kSceneWidth + sourceX];
  const uint16_t near = layers.near.pixels[nearY * data::kSceneWidth + sourceX];
  if (middle != data::kSceneTransparentKey) pixel = middle;
  if (near != data::kSceneTransparentKey) pixel = near;
  return pixel;
}
bool sceneRegion(Route route, uint64_t visualSteps, int left, int right, int top, int bottom) {
  for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
    const uint16_t expected = expectedScenePixel(route, x, y, visualSteps);
    const uint16_t actual = M5.Display.readPixel(x, y);
    if (actual != expected) {
      std::fprintf(stderr, "scene mismatch route=%u steps=%llu x=%d y=%d expected=%04x actual=%04x\n",
                   unsigned(route), static_cast<unsigned long long>(visualSteps), x, y, expected, actual);
      return false;
    }
  }
  return true;
}
bool farScene(Route route, uint64_t visualSteps) {
  return sceneRegion(route, visualSteps, 50, 150, 110, 250) &&
         sceneRegion(route, visualSteps, 316, 416, 110, 250);
}
bool groundScene(Route route, uint64_t visualSteps) {
  return sceneRegion(route, visualSteps, 50, 150, 270, 350) &&
         sceneRegion(route, visualSteps, 316, 416, 270, 350);
}
void sceneDepthMotion() {
  const char* names[] = {"forest", "coast", "cave"};
  for (unsigned route = 0; route < 3; ++route) {
    MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    if (route) {
      openMenu(runtime); select(runtime, 1); select(runtime, route); settle(runtime); home(runtime);
    }
    check(farScene(Route(route), 0) && groundScene(Route(route), 0), "selected route starts at its static scene");
    idle(runtime, 700);
    check(farScene(Route(route), 0) && groundScene(Route(route), 0), "time without steps does not move the scene");
    walk(runtime, platform, 4);
    uint64_t visualSteps = runtime.engine().view().totalSteps;
    check(visualSteps > 0, "real IMU input advances visual scene steps");
    check(farScene(Route(route), visualSteps) && groundScene(Route(route), visualSteps),
          "ground moves down at both layer rates while the far scene stays fixed");
    char name[48]; std::snprintf(name, sizeof(name), "home-depth-%s-moving", names[route]); screenshot(runtime, name);
    idle(runtime, 700);
    check(farScene(Route(route), visualSteps) && groundScene(Route(route), visualSteps),
          "stopping IMU input freezes the complete scene position");
    std::snprintf(name, sizeof(name), "home-depth-%s-stopped", names[route]); screenshot(runtime, name);
    const unsigned height = data::kSceneHeight - data::kRouteSceneLayers[route].groundTop;
    walk(runtime, platform, height - visualSteps % height);
    visualSteps = runtime.engine().view().totalSteps;
    check(visualSteps % height == 0, "real steps reach the complete ground loop");
    check(farScene(Route(route), 0) && groundScene(Route(route), 0) &&
          farScene(Route(route), visualSteps) && groundScene(Route(route), visualSteps),
          "complete ground loop restores the exact static pixels");
    std::snprintf(name, sizeof(name), "home-depth-%s-loop", names[route]); screenshot(runtime, name);
  }
}
void sceneDepthNavigationAndSleep() {
  MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  walk(runtime, platform, 3);
  const uint64_t forestSteps = runtime.engine().view().totalSteps;
  openMenu(runtime); walk(runtime, platform, 2); select(runtime, 7); settle(runtime); home(runtime);
  check(groundScene(Route::Forest, forestSteps), "steps consumed in menus do not jump the home scene");
  openMenu(runtime); select(runtime, 1); select(runtime, 1); settle(runtime); home(runtime);
  check(farScene(Route::Coast, 0) && groundScene(Route::Coast, 0), "route change resets depth to the static coast");
  screenshot(runtime, "home-depth-route-reset");
  const uint64_t beforeCoast = runtime.engine().view().totalSteps;
  walk(runtime, platform, 2);
  const uint64_t coastSteps = runtime.engine().view().totalSteps - beforeCoast;
  check(groundScene(Route::Coast, coastSteps), "new route accumulates depth from zero");
  idle(runtime, 30020); check(runtime.dimmed(), "depth scene enters power saving");
  walk(runtime, platform, 3);
  edge(runtime, false, true); home(runtime);
  check(groundScene(Route::Coast, coastSteps), "sleep steps do not jump the resumed scene");
  screenshot(runtime, "home-depth-after-wake");
  openMenu(runtime); select(runtime, 1); select(runtime, 2); settle(runtime); home(runtime);
  check(farScene(Route::Cave, 0) && groundScene(Route::Cave, 0), "wake and later route change preserve reset semantics");
}
}
