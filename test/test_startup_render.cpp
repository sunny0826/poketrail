#include "support/progression_ui.hpp"
#include "support/rendered_text.hpp"
#include "icons_generated.hpp"
#include "scene_backgrounds_generated.hpp"
using namespace progression_ui;

namespace {
bool advanceUntilDisplayChanges(Runtime& runtime, const std::vector<uint16_t>& initial,
                                uint32_t maxMs, uint32_t& elapsedOut) {
  elapsedOut = 0;
  while (elapsedOut < maxMs) {
    frame(runtime, 20);
    elapsedOut += 20;
    if (M5.Display.framebuffer() != initial) return true;
  }
  return false;
}

void starterAndEncounterFrameTiming() {
  {
    MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    select(runtime, 1);
    check(runtime.screen().page == Page::Starter && runtime.screen().selected == 0,
          "new game reaches starter selection");
    const auto starterFrame0 = M5.Display.framebuffer();
    screenshot(runtime, "starter-bulbasaur-frame0", 0);
    uint32_t elapsed = 0;
    check(advanceUntilDisplayChanges(runtime, starterFrame0, 460, elapsed) && elapsed < 500,
          "starter preview advances on sprite frame duration before 500ms");
    screenshot(runtime, "starter-bulbasaur-next", 0);

    edge(runtime, false, true);
    check(runtime.screen().page == Page::Starter && runtime.screen().selected == 1,
          "starter selection moves to Charmander");
    const auto charmander0 = M5.Display.framebuffer();
    check(charmander0 != starterFrame0, "switching starter updates preview sprite");
    check(advanceUntilDisplayChanges(runtime, charmander0, 460, elapsed) && elapsed < 500,
          "switched starter continues animating on sprite frame duration before 500ms");
    screenshot(runtime, "starter-charmander-next", 0);
  }
  {
    MemoryStore store;
    Save save;
    save.bootGeneration = 1; save.starter = save.partner = 1;
    setOwned(save, 1); setRegistered(save, 1); setRegistered(save, 25);
    save.rosterRevision = 1; save.encounterTarget = 300;
    save.totalSteps = save.undatedSteps = 500; save.explorationSpent = 500;
    save.nextEncounterId = 2;
    save.phase = Phase::EncounterReady;
    save.encounter.id = 1; save.encounter.species = 25; save.encounter.partner = 1;
    save.encounter.route = Route::Kanto;
    check(validate(save) && store.commit(save) == CommitStatus::Committed,
          "encounter fixture validates and encodes");

    HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    openMenu(runtime);
    select(runtime, MenuItem::Encounter);
    check(runtime.screen().page == Page::Encounter, "encounter menu opens Encounter page");
    const auto encounterFrame0 = M5.Display.framebuffer();
    screenshot(runtime, "encounter-pikachu-frame0", 0);
    uint32_t elapsed = 0;
    check(advanceUntilDisplayChanges(runtime, encounterFrame0, 460, elapsed) && elapsed < 500,
          "encounter preview advances on sprite frame duration before 500ms");
    screenshot(runtime, "encounter-pikachu-next", 0);
  }
}

void thumbnailAndZeroAngleBallPixels() {
  {
    MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    select(runtime, 1);
    select(runtime, 0);
    openMenu(runtime);
    select(runtime, MenuItem::Partner);
    select(runtime, 0);
    check(runtime.screen().page == Page::Partners && runtime.screen().selected == 0,
          "partners page renders owned starter");

    const auto& icon = data::kPokemonIcons[0];
    const int left = 233 - int(icon.width) * 2 / 2;
    const int top = 137 - int(icon.height) * 2 / 2;
    unsigned outlinePreserved = 0;
    for (unsigned row = 0; row < icon.height; ++row) {
      for (unsigned col = 0; col < icon.width; ++col) {
        const uint16_t expected = icon.pixels[row * icon.width + col];
        for (int dy = 0; dy < 2; ++dy) {
          for (int dx = 0; dx < 2; ++dx) {
            const uint16_t actual = M5.Display.readPixel(left + int(col) * 2 + dx,
                                                         top + int(row) * 2 + dy);
            check(actual != kTransparentKey, "2x thumbnail never leaks transparent key");
            if (expected != kTransparentKey) {
              check(actual == expected, "2x thumbnail block matches source icon pixel");
            } else {
              check(actual == theme::kBackground || actual == theme::kOutline,
                    "2x thumbnail transparent cell preserves underlying background and outline");
              if (actual == theme::kOutline) ++outlinePreserved;
            }
          }
        }
      }
    }
    check(outlinePreserved > 0, "2x thumbnail transparency preserves ring outline pixels");
    screenshot(runtime, "partners-thumbnail-2x", 0);
  }
  {
    MemoryStore store;
    Save save;
    save.bootGeneration = 1; save.starter = save.partner = 1;
    setOwned(save, 1); setRegistered(save, 1);
    save.rosterRevision = 1; save.encounterTarget = 300;
    save.totalSteps = save.undatedSteps = 5000; save.explorationSpent = 5000;
    save.nextEncounterId = 2;
    save.phase = Phase::ThrowReady; save.result = Result::Won;
    save.encounter.id = 1; save.encounter.species = 1; save.encounter.partner = 1;
    save.encounter.route = Route::Kanto;
    check(validate(save) && store.commit(save) == CommitStatus::Committed,
          "throw fixture validates and encodes");

    HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    check(runtime.screen().page == Page::Throw && !runtime.screen().capture.active,
          "throw ready enters Throw aim screen");

    const auto& ball = data::kPokeBallIcon;
    const auto& dimmedScene = data::kRegionScenesDim[regionIndex(Route::Kanto)];
    const int centers[][2] = {{ballSlotX(0), kBallSlotY}, {ballSlotX(1), kBallSlotY}, {233, kHandY}};
    for (unsigned c = 0; c < 3; ++c) {
      const int bx = centers[c][0], by = centers[c][1];
      const int left = bx - int(ball.width) / 2;
      const int top = by - int(ball.height) / 2;
      for (unsigned row = 0; row < ball.height; ++row) {
        for (unsigned col = 0; col < ball.width; ++col) {
          const int px = left + int(col);
          const int py = top + int(row);
          const uint16_t expected = ball.pixels[row * ball.width + col];
          const uint16_t actual = M5.Display.readPixel(px, py);
          check(actual != kTransparentKey, "zero-angle ball never leaks transparent key");
          if (expected != kTransparentKey) {
            check(actual == expected, "zero-angle ball matches kPokeBallIcon pixel");
          } else if (py < 92) {
            check(actual == theme::kBackground,
                  "zero-angle slot ball transparent corner preserves header background");
          } else if (c == 2) {
            const unsigned sx = unsigned(px - 5) / 2;
            const unsigned sy = unsigned(py - 92) / 2;
            const uint16_t scenePixel = sceneEdgeVisible(sx, sy)
                ? dimmedScene.pixels[sy * data::kSceneWidth + sx]
                : theme::kBackground;
            check(actual == scenePixel,
                  "zero-angle hand ball transparent corner preserves dimmed route scene");
          }
        }
      }
    }
    screenshot(runtime, "throw-ball-zero-angle", 0);
  }
}
}

int main() {
  c152::begin("startup render");
  Screens unavailable;
  check(!unavailable.startup(0), "missing graphics buffer reports failure without crashing");
  drawGraphicsError({}, i18n::Language::Chinese);
  check(rendered_text::contains("画面不可用，请重启", theme::kRed),
        "graphics failure remains readable without a sprite buffer");
  starterAndEncounterFrameTiming();
  thumbnailAndZeroAngleBallPixels();
  std::printf("%u startup and preview render checks passed\n", checks);
}
