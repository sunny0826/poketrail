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
const uint8_t pixels_raw[] = {1, 2, 0, 3, 4, 5};
const uint8_t data[] = {120,156,99,100,98,96,102,97,5,0,0,44,0,16};
const Frame frames[] = {{0, 100}, {0, 200}, {0, 50}};
const Sprite sample = {"test", "", 1, 3, 2, palette, 6, data, sizeof(data), sizeof(pixels_raw), frames, 3, 350, "", "", false};

void testInputs() {
  check(releaseInput(233,233,238,237,true) == Input::React, "small click jitter tolerated");
  check(releaseInput(233,233,235,130,false) == Input::None, "vertical swipe ignored");
  check(releaseInput(233,233,170,140,false) == Input::None, "mostly vertical drag ignored");
  check(releaseInput(233,233,233,233,false) == Input::None, "hold release is not a tap");
  check(releaseInput(233,65,233,65,true) == Input::None, "title click ignored");

}

void testTimeline() {
  State state;
  update(state, sample, 99);
  check(state.frame_index == 0, "first frame holds original duration");
  update(state, sample, 100);
  check(state.frame_index == 1, "advance at frame boundary");
  update(state, sample, 299);
  check(state.frame_index == 1, "longer duration is retained");
  update(state, sample, 300);
  check(state.frame_index == 2, "short third frame");
  update(state, sample, 350);
  check(state.frame_index == 0, "animation loops continuously");
  update(state, sample, 350 * 100 + 301);
  check(state.frame_index == 2, "delayed loop catches up without accumulating drift");
  state.animation_started_ms = UINT32_MAX - 49;
  update(state, sample, 50);
  check(state.frame_index == 1, "millis wrap preserves frame timing");

}

void testDecode() {
  std::vector<uint16_t> pixels(24, 0xABCD);
  check(decodeFrame(sample, 0, 2, pixels.data(), pixels.size()), "decode and integer upscale");
  bool correct = true;
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 6; ++x)
      if (pixels[y*6+x] != palette[pixels_raw[(y/2)*3+x/2]]) correct = false;
  check(correct, "nearest neighbor retains black white color and transparent cells");
  check(!decodeFrame(sample, 3, 2, pixels.data(), pixels.size()), "out of range frame fails");
  check(!decodeFrame(sample, 0, 0, pixels.data(), pixels.size()), "zero scale fails");
  check(!decodeFrame(sample, 0, 2, pixels.data(), 23), "short output fails");
  check(!decodeFrame(sample, 0, 2, nullptr, 24), "null output fails");
  const Frame out_of_bounds[] = {{6, 100}};
  Sprite broken = sample;
  broken.frames = out_of_bounds;
  check(!decodeFrame(broken, 0, 1, pixels.data(), 24), "out of blob frame fails");
}
}

int main() {
  testInputs();
  testTimeline();
  testDecode();
  std::printf("%d viewer checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
