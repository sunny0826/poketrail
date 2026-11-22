#include <cstdio>
#include <vector>
#include <zlib.h>

#include "../firmware/apps/20_pokedex/game.hpp"
#include "../firmware/apps/20_pokedex/menu_buttons.hpp"

using namespace pokedex;
using namespace pokedex::viewer;
using pokedex::ui::MenuAction;
using pokedex::ui::MenuButtons;

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
  check(releaseInput(233,233,251,245,true) == Input::React, "18px click jitter still triggers reaction");
  check(releaseInput(233,233,253,213,true) == Input::React, "20px tolerance boundary triggers reaction");
  check(releaseInput(233,233,258,233,false) == Input::None, "25px short drag ignored");
  check(releaseInput(233,233,258,233,true) == Input::None, "25px drift beyond tolerance is not a click");
  check(releaseInput(233,233,188,233,false) == Input::Next, "45px left swipe advances");
  check(releaseInput(233,233,278,233,false) == Input::Previous, "45px right swipe goes back");
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

  struct SizeCase { uint16_t w, h; const char* label; };
  const SizeCase sizes[] = {
    {5, 3, "odd x odd"},
    {4, 3, "even x odd"},
    {3, 4, "odd x even"},
    {4, 4, "even x even"},
  };
  for (const auto& sz : sizes) {
    const size_t framePixels = size_t(sz.w) * sz.h;
    const uint16_t kFrames = 3;
    std::vector<uint8_t> raw(framePixels * kFrames);
    std::vector<Frame> multiFrames(kFrames);
    for (uint16_t f = 0; f < kFrames; ++f) {
      multiFrames[f] = {uint32_t(f * framePixels), uint16_t(60 + f * 20)};
      for (size_t i = 0; i < framePixels; ++i)
        raw[f * framePixels + i] = uint8_t((i * 5 + f * 3) % 6);
    }
    std::vector<uint8_t> compressed(compressBound(uLong(raw.size())));
    uLongf compSize = compressed.size();
    check(compress(compressed.data(), &compSize, raw.data(), uLong(raw.size())) == Z_OK,
          "multi-frame fixture compresses");
    compressed.resize(compSize);
    const Sprite multi = {"multi", "", 1, sz.w, sz.h, palette, 6,
                          compressed.data(), uint32_t(compSize), uint32_t(raw.size()),
                          multiFrames.data(), kFrames, 240, "", "", false};
    for (uint16_t scale : {uint16_t(1), uint16_t(2), uint16_t(3)}) {
      const size_t outW = size_t(sz.w) * scale;
      const size_t outH = size_t(sz.h) * scale;
      std::vector<uint16_t> out(outW * outH, 0xABCD);
      for (uint16_t f = 0; f < kFrames; ++f) {
        check(decodeFrame(multi, f, scale, out.data(), out.size()),
              "multi-frame odd/even sprite decodes across scales");
        bool exact = true;
        for (size_t y = 0; y < outH && exact; ++y)
          for (size_t x = 0; x < outW; ++x) {
            const uint8_t idx = raw[f * framePixels + (y / scale) * sz.w + (x / scale)];
            if (out[y * outW + x] != palette[idx]) { exact = false; break; }
          }
        check(exact, sz.label);
      }
    }
  }

  std::vector<uint8_t> badRaw = {0, 1, 6, 2, 3, 4};
  std::vector<uint8_t> badComp(compressBound(uLong(badRaw.size())));
  uLongf badCompSize = badComp.size();
  check(compress(badComp.data(), &badCompSize, badRaw.data(), uLong(badRaw.size())) == Z_OK,
        "invalid palette fixture compresses");
  const Frame badFrame[] = {{0, 100}};
  const Sprite badSprite = {"bad", "", 1, 3, 2, palette, 6,
                            badComp.data(), uint32_t(badCompSize), uint32_t(badRaw.size()),
                            badFrame, 1, 100, "", "", false};
  check(!decodeFrame(badSprite, 0, 2, pixels.data(), pixels.size()),
        "palette index out of range fails");
}

void testMenuButtons() {
  MenuButtons b;
  // Short press moves once on release; staggered <350ms chord confirms.
  check(b.update(false, true, 100, true) == MenuAction::None, "press waits for release");
  check(b.update(false, true, 449, true) == MenuAction::None, "no repeat before 350ms");
  check(b.update(false, false, 449, true) == MenuAction::Next, "short release steps once");

  check(b.update(true, false, 1000, true) == MenuAction::None, "left press waits");
  check(b.update(true, false, 1349, true) == MenuAction::None, "still before repeat threshold");
  check(b.update(true, true, 1349, true) == MenuAction::Confirm, "overlap before 350ms confirms");
  check(b.update(true, false, 1360, true) == MenuAction::None, "consumed after confirm");
  check(b.update(false, false, 1380, true) == MenuAction::None, "full release resets consumed");

  // Repeat at 350ms, 120ms cadence, then 65ms past 1200ms; release does not add an extra step.
  uint32_t t = 2000;
  check(b.update(false, true, t, true) == MenuAction::None, "start repeat hold");
  check(b.update(false, true, t + 349, true) == MenuAction::None, "349ms does not repeat");
  check(b.update(false, true, t + 350, true) == MenuAction::Next, "350ms triggers 1st repeat");
  check(b.update(false, true, t + 469, true) == MenuAction::None, "119ms after repeat waits");
  check(b.update(false, true, t + 470, true) == MenuAction::Next, "120ms triggers 2nd repeat");
  for (uint32_t stepAt : {590u, 710u, 830u, 950u, 1070u, 1190u}) {
    check(b.update(false, true, t + stepAt, true) == MenuAction::Next, "120ms repeat cadence");
  }
  check(b.update(false, true, t + 1254, true) == MenuAction::None, "64ms after 1190ms waits");
  check(b.update(false, true, t + 1255, true) == MenuAction::Next, "past 1200ms accelerates to 65ms");
  check(b.update(false, true, t + 1320, true) == MenuAction::Next, "65ms cadence continues");
  check(b.update(false, false, t + 1330, true) == MenuAction::None, "release after repeat adds no extra step");
  check(b.update(true, false, t + 1340, true) == MenuAction::None &&
        b.update(false, false, t + 1360, true) == MenuAction::Previous,
        "fresh press works immediately after repeat release");

  // Pressing the other key during repeat aborts safely into Consumed without Confirm or release step.
  t = UINT32_MAX - 500;
  check(b.update(true, false, t, true) == MenuAction::None, "wraparound press");
  check(b.update(true, false, t + 350, true) == MenuAction::Previous, "wraparound triggers repeat");
  check(b.update(true, true, t + 400, true) == MenuAction::None, "other key during repeat cancels without confirm");
  check(b.update(false, true, t + 420, true) == MenuAction::None, "partial release stays consumed");
  check(b.update(false, false, t + 440, true) == MenuAction::None, "final release stays silent");

  // repeatAllowed == false keeps 30s hold silent and allows slow overlapping confirm.
  check(b.update(true, false, 10000, false) == MenuAction::None, "non-repeat press");
  check(b.update(true, false, 40100, false) == MenuAction::None, "30s hold never repeats when disabled");
  check(b.update(true, true, 40120, false) == MenuAction::Confirm, "slow overlap still confirms when disabled");
  check(b.update(false, false, 40140, false) == MenuAction::None, "release after slow confirm");
}
}

int main() {
  testInputs();
  testTimeline();
  testDecode();
  testMenuButtons();
  std::printf("%d viewer checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
