#include <cstdio>
#include <vector>

#include "../firmware/apps/20_pokedex/game.hpp"

using namespace pokedex;
using namespace pokedex::viewer;

namespace {
int checks = 0, failures = 0;
void check(bool condition, const char* message) {
  ++checks;
  if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}

const uint16_t palette[] = {kTransparentKey, 0, 0xFFFF, 0xF800, 0x07E0, 0x001F,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const uint8_t data[] = {0x01, 0x02, 0x00, 0x03, 0x04, 0x05};
const Frame frames[] = {{0, 6, 100}, {0, 6, 200}, {0, 6, 50}};
const Sprite sample = {"test", "", 1, 3, 2, palette, data, sizeof(data), frames, 3, 350, "", "", false};

void testInputs() {
  check(releaseInput(233,233,233,233,true) == Input::React, "central click reacts");
  check(releaseInput(233,233,238,237,true) == Input::React, "small click jitter tolerated");
  check(releaseInput(233,233,180,237,false) == Input::Next, "left swipe goes next");
  check(releaseInput(233,233,290,230,false) == Input::Previous, "right swipe goes previous");
  check(releaseInput(233,233,253,237,false) == Input::None, "short drag is not a tap");
  check(releaseInput(233,233,235,130,false) == Input::None, "vertical swipe ignored");
  check(releaseInput(233,233,170,140,false) == Input::None, "mostly vertical drag ignored");
  check(releaseInput(233,233,233,233,false) == Input::None, "hold release is not a tap");
  check(releaseInput(233,65,233,65,true) == Input::None, "title click ignored");
  check(releaseInput(233,233,188,233,false) == Input::Next, "exact swipe threshold accepted");

  State state;
  applyInput(state, Input::Previous, 151, 100);
  check(state.sprite_index == 150, "previous wraps from first to last");
  applyInput(state, Input::Next, 151, 200);
  check(state.sprite_index == 0, "next wraps from last to first");
  for (unsigned i = 1; i <= 151; ++i) {
    applyInput(state, Input::Next, 151, i * 10);
    check(state.sprite_index == i % 151, "every entry reachable in order");
  }
  check(!applyInput(state, Input::None, 151, 0), "no input does not redraw");
  check(!applyInput(state, Input::Next, 0, 0), "empty catalog cannot navigate");
}

void testTimeline() {
  State state;
  check(!update(state, sample, 99) && state.frame_index == 0, "first frame holds original duration");
  check(update(state, sample, 100) && state.frame_index == 1, "advance at frame boundary");
  check(!update(state, sample, 299), "longer duration is retained");
  check(update(state, sample, 300) && state.frame_index == 2, "short third frame");
  check(update(state, sample, 350) && state.frame_index == 0, "animation loops continuously");
  update(state, sample, 350 * 100 + 301);
  check(state.frame_index == 2, "delayed loop catches up without accumulating drift");
  state.animation_started_ms = UINT32_MAX - 49;
  update(state, sample, 50);
  check(state.frame_index == 1, "millis wrap preserves frame timing");

  state = State{};
  update(state, sample, 300);
  applyInput(state, Input::React, 151, 300);
  check(state.frame_index == 2 && state.animation_started_ms == 0,
        "tap preserves ongoing animation phase");
  update(state, sample, 540);
  check(state.reacting && state.jump == 14 && state.frame_index == 1,
        "reaction jumps while original animation continues");
  update(state, sample, 780);
  check(state.reacting && state.jump == 0, "jump returns to center");
  check(update(state, sample, 1100) && !state.reacting && state.jump == 0,
        "reaction expires and requests redraw to erase heart");
  applyInput(state, Input::React, 151, 1200);
  applyInput(state, Input::Next, 151, 1220);
  check(!state.reacting && state.jump == 0 && state.frame_index == 0 &&
        state.animation_started_ms == 1220, "swipe cancels response and starts new animation");
}

void testDisplayScale() {
  Sprite small = sample;
  small.width = 41;
  small.height = 42;
  Sprite enlarged = sample;
  enlarged.width = 87;
  enlarged.height = 89;
  check(displayScale(small) == 2 && displayScale(enlarged) == 2,
        "forms outside the enlarged category keep two-pixel blocks");
  enlarged.enlarged = true;
  check(displayScale(enlarged) == 3 && enlarged.width * displayScale(enlarged) == 261 &&
        enlarged.height * displayScale(enlarged) == 267,
        "Charizard-sized final evolution enlarges to 261 by 267");
  check(small.height * displayScale(small) < enlarged.height * displayScale(enlarged),
        "final evolution stays larger than its base form");
  enlarged.width = 86;
  enlarged.height = 71;
  check(displayScale(enlarged) == 3, "Venusaur-sized final evolution enlarges");
  enlarged.width = 66;
  enlarged.height = 56;
  check(displayScale(enlarged) == 3 && enlarged.width * displayScale(enlarged) == 198 &&
        enlarged.height * displayScale(enlarged) == 168,
        "Charmeleon-sized intermediate form also enlarges to 198 by 168");
  enlarged.width = 112;
  enlarged.height = 90;
  check(displayScale(enlarged) == 3, "largest safe three-times canvas is accepted");
  enlarged.width = 113;
  check(displayScale(enlarged) == 2, "wide final evolution falls back without clipping");
  enlarged.width = 82;
  enlarged.height = 105;
  check(displayScale(enlarged) == 2, "Fearow keeps its complete tall animation at two-times");
  enlarged.height = 117;
  check(displayScale(enlarged) == 2, "Pidgeotto intermediate keeps its tall full animation");
  enlarged.width = 45;
  enlarged.height = 49;
  check(displayScale(enlarged) == 3 && enlarged.width * displayScale(enlarged) == 135 &&
        enlarged.height * displayScale(enlarged) == 147,
        "Farfetch'd-sized standalone species enlarges to 135 by 147");
  enlarged.width = 78;
  enlarged.height = 61;
  check(displayScale(enlarged) == 3 && enlarged.width * displayScale(enlarged) == 234 &&
        enlarged.height * displayScale(enlarged) == 183,
        "Kangaskhan-sized standalone species enlarges to 234 by 183");
  enlarged.width = 90;
  enlarged.height = 69;
  check(displayScale(enlarged) == 3 && enlarged.width * displayScale(enlarged) == 270 &&
        enlarged.height * displayScale(enlarged) == 207,
        "Mewtwo-sized standalone species enlarges to 270 by 207");
  enlarged.width = 128;
  enlarged.height = 97;
  check(displayScale(enlarged) == 2 && enlarged.width * displayScale(enlarged) == 256 &&
        enlarged.height * displayScale(enlarged) == 194,
        "Moltres-sized standalone species falls back to 256 by 194");
  enlarged.width = 153;
  enlarged.height = 94;
  check(displayScale(enlarged) == 2, "Lugia keeps its wide standalone animation at two-times");
  enlarged.width = pokedex::kMaxSourceSize;
  enlarged.height = 117;
  check(displayScale(enlarged) == 2, "largest source canvas fits without clipping");
  enlarged.width = pokedex::kMaxSourceSize + 1;
  check(displayScale(enlarged) == 0, "oversized canvas fails rather than changing relative proportions");
  small.width = 0;
  check(displayScale(small) == 0, "zero-width canvas has no valid scale");
  small.width = 41;
  small.height = 0;
  check(displayScale(small) == 0, "zero-height canvas has no valid scale");
}

void testDecode() {
  std::vector<uint16_t> pixels(24, 0xABCD);
  check(decodeFrame(sample, 0, 2, pixels.data(), pixels.size()), "decode and integer upscale");
  bool correct = true;
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 6; ++x)
      if (pixels[y*6+x] != palette[data[(y/2)*3+x/2]]) correct = false;
  check(correct, "nearest neighbor retains black white color and transparent cells");
  pixels.resize(54);
  check(decodeFrame(sample, 0, 3, pixels.data(), pixels.size()), "decode three-times blocks");
  correct = true;
  for (int y = 0; y < 6; ++y)
    for (int x = 0; x < 9; ++x)
      if (pixels[y*9+x] != palette[data[(y/3)*3+x/3]]) correct = false;
  check(correct, "three-times blocks preserve every source color and transparency");
  check(!decodeFrame(sample, 3, 2, pixels.data(), pixels.size()), "out of range frame fails");
  check(!decodeFrame(sample, 0, 0, pixels.data(), pixels.size()), "zero scale fails");
  check(!decodeFrame(sample, 0, 2, pixels.data(), 23), "short output fails");
  check(!decodeFrame(sample, 0, 2, nullptr, 24), "null output fails");
  const Frame truncated[] = {{0, 2, 100}};
  Sprite broken = sample;
  broken.frames = truncated;
  check(!decodeFrame(broken, 0, 1, pixels.data(), 24), "truncated frame fails");
  const uint8_t overflow[] = {0xF1};
  const Frame excessive[] = {{0, 1, 100}};
  broken.data = overflow;
  broken.data_size = 1;
  broken.frames = excessive;
  check(!decodeFrame(broken, 0, 1, pixels.data(), 24), "excessive run fails");
  const Frame out_of_bounds[] = {{1, 1, 100}};
  broken.frames = out_of_bounds;
  check(!decodeFrame(broken, 0, 1, pixels.data(), 24), "out of blob frame fails");
}
}

int main() {
  testInputs();
  testTimeline();
  testDisplayScale();
  testDecode();
  std::printf("%d viewer checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
