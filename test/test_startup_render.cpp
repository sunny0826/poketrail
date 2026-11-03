#include "support/progression_ui.hpp"
#include "sprites_generated.hpp"

using namespace progression_ui;
namespace {
void resetInput() {
  M5.BtnA = m5::Button_Class(); M5.BtnB = m5::Button_Class();
  buttons(false, false); M5.powerDown(false); M5.Touch.detail = {};
  c152::update(); c152::touchWasPressed();
}
void seed(MemoryStore& store) {
  Engine engine(store); engine.begin(); engine.chooseStarter(1); engine.checkpoint();
}
void illustration(uint32_t elapsed) {
  const auto& sprite = data::kSprites[0];
  viewer::State playback;
  viewer::update(playback, sprite, elapsed);
  std::vector<uint16_t> expected(sprite.width * 3 * sprite.height * 3);
  check(decodeFrame(sprite, playback.frame_index, 3, expected.data(), expected.size()), "decode source frame");
  unsigned visible = 0;
  for (int y = 0; y < sprite.height * 3; ++y) for (int x = 0; x < sprite.width * 3; ++x) {
    const auto pixel = expected[y * sprite.width * 3 + x];
    if (pixel == kTransparentKey) continue;
    ++visible;
    check(M5.Display.readPixel(233 - sprite.width * 3 / 2 + x, 207 - sprite.height * 3 / 2 + y) == pixel,
          "startup preserves source pixels, integer scaling and animation timing");
  }
  check(visible > 1000, "Bulbasaur is visible at useful size");
}
void normalBoot() {
  resetInput(); host_time_ms = 0;
  MemoryStore store; seed(store); HostPlatform platform; Runtime runtime(store, platform);
  const auto original = store.bytes; const unsigned commits = store.commits;
  check(runtime.startStartup(), "startup draws before storage loads");
  check(store.bytes == original && store.commits == commits, "drawing startup never writes storage");
  illustration(0);
  check(runtime.begin(), "begin reuses startup graphics buffer");
  const auto bootSave = store.bytes; const unsigned bootCommits = store.commits;
  screenshot(runtime, "startup-first", 40);
  illustration(40);
  const auto first = M5.Display.framebuffer();
  screenshot(runtime, "startup-animation", 400);
  illustration(440);
  check(first != M5.Display.framebuffer(), "animation advances during startup");
  buttons(true, true); idle(runtime, 740);
  check(runtime.startupActive(), "startup remains visible before 1200ms");
  check(store.bytes == bootSave && store.commits == bootCommits, "splash ticks add no save writes");
  check(runtime.metrics().samples > 30, "IMU sampling continues during startup");
  M5.powerClick(); frame(runtime);
  check(!runtime.startupActive() && !runtime.dimmed() && runtime.screen().page == Page::Home,
        "1200ms ends splash without executing held controls or power edge");
  buttons(false, false); frame(runtime); frame(runtime);
  check(runtime.screen().page == Page::Home, "release cannot enter the menu");
  screenshot(runtime, "startup-home");
  openMenu(runtime); check(runtime.screen().page == Page::Menu, "fresh input works after release");
}
void touchAndWrap() {
  resetInput(); host_time_ms = UINT32_MAX - 500;
  MemoryStore store; seed(store); HostPlatform platform; Runtime runtime(store, platform);
  check(runtime.startStartup() && runtime.begin(), "startup accepts wrapping clock");
  M5.Touch.detail.base_x = M5.Touch.detail.x = 233;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 233;
  M5.Touch.detail.pressed = true;
  idle(runtime, 1180); check(runtime.startupActive(), "wrap does not end startup early");
  idle(runtime, 100); check(!runtime.startupActive(), "wrap still expires startup");
  M5.Touch.detail.pressed = false; M5.Touch.detail.clicked = M5.Touch.detail.released = true;
  frame(runtime); M5.Touch.detail = {}; frame(runtime);
  check(runtime.screen().page == Page::Home, "held touch release does not click through");
  tap(runtime, 233, 233); check(runtime.screen().page == Page::Menu, "new touch opens menu");
}
class FaultStore : public MemoryStore {
 public:
  LoadStatus status = LoadStatus::Corrupt;
  LoadResult load(Save&) override { return LoadResult(status); }
};
void failedBoot() {
  for (auto status : {LoadStatus::Corrupt, LoadStatus::NeedsProvisioning, LoadStatus::IoError}) {
    resetInput(); host_time_ms = 0;
    FaultStore store; store.status = status; HostPlatform platform; Runtime runtime(store, platform);
    check(runtime.startStartup() && runtime.begin(), "storage failure still has graphics");
    screenshot(runtime, status == LoadStatus::Corrupt ? "startup-before-corrupt" :
                        status == LoadStatus::NeedsProvisioning ? "startup-before-unmounted" : "startup-before-io-error", 40);
    idle(runtime, 1160);
    check(!runtime.startupActive() && runtime.screen().page == Page::Error, "storage failure ends in error page");
    check(store.commits == 0, "failed boot never overwrites or initializes storage");
    screenshot(runtime, status == LoadStatus::Corrupt ? "startup-corrupt" :
                        status == LoadStatus::NeedsProvisioning ? "startup-unmounted" : "startup-io-error");
  }
  Screens unavailable;
  check(!unavailable.startup(0), "missing graphics buffer reports failure without crashing");
  drawGraphicsError({}, i18n::Language::Chinese);
  check(glyph(i18n::text(i18n::Language::Chinese, i18n::Text::GraphicsError), 290, theme::kRed),
        "graphics error remains readable without a sprite buffer");
}
void slowLoadAndRebuild() {
  resetInput(); host_time_ms = 0;
  MemoryStore store; seed(store); HostPlatform platform;
  {
    Runtime runtime(store, platform);
    check(runtime.startStartup(), "draw before slow mount");
    host_time_ms += 2000;
    check(runtime.begin(), "begin after slow mount"); frame(runtime);
    check(!runtime.startupActive() && runtime.screen().page == Page::Home, "slow mount does not add another delay");
  }
  Runtime rebuilt(store, platform); start(rebuilt);
  check(!rebuilt.startupActive() && rebuilt.screen().page == Page::Home, "runtime rebuild skips cold-boot splash");
}
}
int main() {
  c152::begin("startup render");
  normalBoot(); touchAndWrap(); failedBoot(); slowLoadAndRebuild();
  std::printf("%u startup checks passed\n", checks);
}
