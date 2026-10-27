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
std::vector<uint16_t> groundPixels() {
  std::vector<uint16_t> pixels;
  for (int y = 270; y < 350; ++y) for (int x : {50, 316}) {
    for (int offset = 0; offset < 100; ++offset) pixels.push_back(M5.Display.readPixel(x + offset, y));
  }
  return pixels;
}
uint64_t defaultSceneSteps(uint32_t origin) { return (host_time_ms - origin) / 100; }
void sceneDepthMotion() {
  const char* names[] = {"forest", "coast", "cave"};
  for (unsigned route = 0; route < 3; ++route) {
    MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    if (route) {
      openMenu(runtime); select(runtime, 1); select(runtime, route); home(runtime);
    }
    const uint32_t origin = host_time_ms;
    check(farScene(Route(route), 0) && groundScene(Route(route), 0), "selected route starts at its static scene");
    idle(runtime, 700);
    uint64_t visualSteps = defaultSceneSteps(origin);
    check(visualSteps == 7 && farScene(Route(route), visualSteps) && groundScene(Route(route), visualSteps),
          "home animates by default without any walking input");
    walk(runtime, platform, 4);
    visualSteps = defaultSceneSteps(origin);
    check(runtime.engine().view().totalSteps > 0, "walking still updates the Engine step account");
    check(farScene(Route(route), visualSteps) && groundScene(Route(route), visualSteps),
          "walking does not accelerate the default scene timeline");
    char name[48]; std::snprintf(name, sizeof(name), "home-depth-%s-moving", names[route]); screenshot(runtime, name);
    idle(runtime, 700);
    visualSteps = defaultSceneSteps(origin);
    check(farScene(Route(route), visualSteps) && groundScene(Route(route), visualSteps),
          "default motion continues after walking stops");
    std::snprintf(name, sizeof(name), "home-depth-%s-default", names[route]); screenshot(runtime, name);
    visualSteps = defaultSceneSteps(origin);
    const unsigned height = data::kSceneHeight - data::kRouteSceneLayers[route].groundTop;
    idle(runtime, (height - visualSteps % height) * 100);
    visualSteps = defaultSceneSteps(origin);
    check(visualSteps % height == 0, "default timeline reaches the complete ground loop");
    check(farScene(Route(route), 0) && groundScene(Route(route), 0) &&
          farScene(Route(route), visualSteps) && groundScene(Route(route), visualSteps),
          "complete ground loop restores the exact static pixels");
    std::snprintf(name, sizeof(name), "home-depth-%s-loop", names[route]); screenshot(runtime, name);
  }
}
void sceneDepthNavigationAndSleep() {
  MemoryStore store; ready(store); HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  const uint32_t forestOrigin = host_time_ms;
  walk(runtime, platform, 3);
  const uint64_t forestSteps = defaultSceneSteps(forestOrigin);
  openMenu(runtime); walk(runtime, platform, 2); select(runtime, 9); home(runtime);
  check(groundScene(Route::Forest, forestSteps), "menu time does not advance or jump the home scene");
  openMenu(runtime); select(runtime, 1); select(runtime, 1); home(runtime);
  check(farScene(Route::Coast, 0) && groundScene(Route::Coast, 0), "route change resets depth to the static coast");
  const uint32_t coastOrigin = host_time_ms;
  screenshot(runtime, "home-depth-route-reset");
  idle(runtime, 700);
  check(groundScene(Route::Coast, defaultSceneSteps(coastOrigin)), "new route starts its default animation from zero");
  idle(runtime, 30020); check(runtime.dimmed(), "depth scene enters power saving");
  const auto sleepingPixels = groundPixels();
  walk(runtime, platform, 3);
  edge(runtime, false, true); home(runtime);
  check(groundPixels() == sleepingPixels, "sleep and wake resume the same default scene position");
  screenshot(runtime, "home-depth-after-wake");
  openMenu(runtime); select(runtime, 1); select(runtime, 2); home(runtime);
  check(farScene(Route::Cave, 0) && groundScene(Route::Cave, 0), "wake and later route change preserve reset semantics");
}
}
