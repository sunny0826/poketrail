#include <cstdio>
#include <cstdlib>
#include <vector>
#include "support/verification_output.hpp"

#include "../firmware/apps/20_pokedex/viewer_screen.cpp"

uint32_t host_time_ms = 0;
HostSerial Serial;
HostM5 M5;
using namespace pokedex::viewer_screen;
void setup() {
  c152::begin("POKEDEX");
  setFeedback([](uint16_t duration, void*) { c152::feedback(0, duration, 70); }, nullptr);
  begin();
}
void loop() { c152::update(); update(millis()); }


namespace {
int failures = 0, checks = 0;
void check(bool condition, const char* message) {
  ++checks;
  if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}

void resetFrame() {
  M5.Display.fillScreen(TFT_BLACK);
  g_state = State{};
  g_screen_drawn = false;
  g_decoded_sprite = g_decoded_frame = g_label_sprite = UINT16_MAX;
  M5.Touch.detail = HostTouchDetail{};
  M5.BtnA = m5::Button_Class{};
  M5.BtnB = m5::Button_Class{};
  M5.button_a_down = M5.button_b_down = false;
  g_ready = true;
  host_time_ms = 0;
}

void compareFreshFrame(const char* message) {
  const auto actual = M5.Display.framebuffer();
  const State target = g_state;
  resetFrame();
  g_state = target;
  render();
  check(actual == M5.Display.framebuffer(), message);
}

void checkPublishedFrame(const char* message) {
  M5.Display.beginObservation();
  check(render(), "frame decodes successfully");
  M5.Display.endObservation();
  const auto& frames = M5.Display.publishedFrames();
  check(frames.size() == 1 && frames[0] == M5.Display.framebuffer(), message);
}

void savePPM(const char* filename) {
  FILE* file = std::fopen(filename, "wb");
  if (!file) { check(false, "open render evidence"); return; }
  std::fprintf(file, "P6\n468 468\n255\n");
  for (int y = 0; y < 468; ++y) {
    for (int x = 0; x < 468; ++x) {
      const uint16_t p = M5.Display.readPixel(x, y);
      const unsigned char rgb[] = {static_cast<unsigned char>((p >> 11) * 255 / 31),
          static_cast<unsigned char>(((p >> 5) & 63) * 255 / 63),
          static_cast<unsigned char>((p & 31) * 255 / 31)};
      std::fwrite(rgb, 1, 3, file);
    }
  }
  std::fclose(file);
}

void checkSwitchAndResponse() {
  resetFrame();
  checkPublishedFrame("initial frame publishes once");
  g_state.frame_index = 5;
  checkPublishedFrame("new animation frame publishes once");
  compareFreshFrame("animation erases previous frame");
  g_state.sprite_index = 145;  // 最大宽度的火焰鸟
  g_state.frame_index = 0;
  render();
  g_state.sprite_index = 24;
  render();
  compareFreshFrame("species switch clears old sprite and labels");
  pokedex::viewer::applyInput(g_state, Input::React, pokedex::data::kSpriteCount, 0);
  pokedex::viewer::update(g_state, pokedex::data::kSprites[24], 240);
  checkPublishedFrame("reaction publishes only complete frame");
  compareFreshFrame("reaction does not leave old sprite behind");
  pokedex::viewer::update(g_state, pokedex::data::kSprites[24], 800);
  render();
  compareFreshFrame("reaction expiry clears heart and jump");
}

void checkFullSprites() {
  size_t enlarged = 0;
  for (size_t i = 0; i < pokedex::data::kSpriteCount; ++i) {
    resetFrame();
    g_state.sprite_index = i;
    g_state.reacting = true;
    g_state.jump = 14;
    const auto& s = pokedex::data::kSprites[i];
    uint16_t scale = pokedex::displayScale(s);
    if (s.height * scale > 220) scale = 2;
    const int w = s.width * scale, h = s.height * scale;
    if (scale == 3) ++enlarged;
    check(w <= 336 && h <= 270 &&
          scale == (s.has_evolved && s.id != 17 && s.id != 22 && s.height * 3 <= 220 ? 3 : 2),
          "intermediate and final forms enlarge within the observation area");
    check(render(), "all species render");
    const int x0 = 233 - w/2, y0 = 215 - h/2 - g_state.jump;
    bool canvas_inside = y0 >= kFieldTop && 215 - h/2 + h <= kFieldBottom && x0 + w <= 403;
    for (const int x : {x0, x0 + w - 1})
      for (const int y : {y0, 215 - h/2 + h - 1})
        if ((x-233)*(x-233) + (y-233)*(y-233) >
            c152::kVisibleRadius*c152::kVisibleRadius) canvas_inside = false;
    check(canvas_inside, "full animation canvas and jump stay clear of labels circle and heart");
    bool complete = true;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const uint16_t color = g_pixels[y*w+x];
        const uint16_t actual = M5.Display.readPixel(x0+x, y0+y);
        if (color != pokedex::kTransparentKey && actual != color) {
          if (complete) std::fprintf(stderr, "sprite mismatch id=%u x=%d y=%d expected=%04x actual=%04x\n",
                                     unsigned(s.id), x0+x, y0+y, color, actual);
          complete = false;
        }
      }
    }
    check(complete, "entire sprite visible at highest response position");
    bool inside = true;
    for (int y = 0; y < 468; ++y) {
      for (int x = 0; x < 468; ++x) {
        const int dx = x - 233, dy = y - 233;
        if (M5.Display.readPixel(x,y) && dx*dx+dy*dy >
            c152::kVisibleRadius*c152::kVisibleRadius) inside = false;
      }
    }
    check(inside, "sprite name type heart and hint inside round screen");
    lgfx::LGFX_Sprite label;
    label.setColorDepth(16); label.createSprite(260, 26); label.fillScreen(TFT_BLACK);
    label.setTextDatum(middle_center); label.setFont(&fonts::efontCN_16);
    label.setTextColor(theme::kBlue); label.drawString(s.types, 130, 13);
    unsigned ink = 0, missing = 0;
    for (int y = 0; y < 26; ++y) for (int x = 0; x < 260; ++x) {
      if (!label.readPixel(x, y)) continue;
      ++ink;
      if (M5.Display.readPixel(103 + x, 340 + y) != label.readPixel(x, y)) ++missing;
    }
    check(ink > 8 && !missing, "complete type label survives reaction and field repaint");
  }
  size_t expectedEnlarged = 0;
  for (size_t i = 0; i < pokedex::data::kSpriteCount; ++i) {
    const auto& sprite = pokedex::data::kSprites[i];
    if (pokedex::displayScale(sprite) == 3 && sprite.height * 3 <= 220) ++expectedEnlarged;
  }
  check(enlarged == expectedEnlarged, "three-times sprites are limited only by the observation area");
}

void checkLargeAnimation() {
  for (const size_t index : {size_t(4), size_t(5), size_t(41), size_t(147)}) {
    resetFrame();
    g_state.sprite_index = index;
    render();
    const auto& s = pokedex::data::kSprites[index];
    const auto labeled = M5.Display.framebuffer();
    pokedex::viewer::applyInput(g_state, Input::React, pokedex::data::kSpriteCount, 0);
    pokedex::viewer::update(g_state, s, 240);
    checkPublishedFrame("large sprite jump publishes one complete frame");
    bool labels_unchanged = true;
    const auto reacted = M5.Display.framebuffer();
    for (int y = 0; y < c152::kScreenSize; ++y) {
      if (y >= kFieldTop && y < kFieldBottom) continue;
      for (int x = 0; x < c152::kScreenSize; ++x)
        if (labeled[y*468+x] != reacted[y*468+x]) labels_unchanged = false;
    }
    check(labels_unchanged, "large sprite jump and clearing preserve real font label pixels");
    compareFreshFrame("large sprite jump clears its old position");
    pokedex::viewer::update(g_state, s, 480);
    render();
    compareFreshFrame("large sprite landing leaves no pixels at jump apex");
    pokedex::viewer::update(g_state, s, 800);
    render();
    compareFreshFrame("large sprite reaction expiry clears its shifted heart");
    g_state.sprite_index = 3;
    g_state.frame_index = 0;
    render();
    compareFreshFrame("switch from enlarged to small sprite clears the complete old canvas");
  }
}

void release(int sx, int sy, int x, int y, bool clicked) {
  auto& d = M5.Touch.detail;
  d.base_x = sx; d.base_y = sy; d.x = x; d.y = y;
  d.released = true; d.clicked = clicked;
  loop();
  d = HostTouchDetail{};
}

void checkInputLoop() {
  resetFrame();
  render();
  host_time_ms = 100;
  release(233,233,233,233,true);
  check(g_state.reacting && M5.Power.vibration > 0, "real loop click starts response and vibration");
  host_time_ms = 200;
  loop();
  check(M5.Power.vibration == 0, "board update stops vibration");
  release(300,233,170,233,false);
  check(g_state.sprite_index == 1 && !g_state.reacting, "real loop swipe advances without tap response");
  release(170,233,300,233,false);
  check(g_state.sprite_index == 0, "real loop opposite swipe returns");
  release(233,233,250,233,false);
  check(g_state.sprite_index == 0 && !g_state.reacting, "short drag is ignored by loop");
  const auto before = M5.Display.framebuffer();
  host_time_ms += 300;
  loop();
  check(before != M5.Display.framebuffer(), "loop advances visible animation without touch");
}

void setButtons(bool left, bool right, uint32_t advance = 20) {
  M5.button_a_down = left;
  M5.button_b_down = right;
  host_time_ms += advance;
  loop();
}

void checkButtonLoop() {
  resetFrame();
  render();
  const auto first = M5.Display.framebuffer();
  M5.Display.beginObservation();
  setButtons(true, false);
  M5.Display.endObservation();
  check(g_state.sprite_index == pokedex::data::kSpriteCount - 1, "left button wraps first to last");
  check(M5.Display.framebuffer() != first && g_label_sprite == pokedex::data::kSpriteCount - 1,
        "button redraws selected sprite and label");
  const auto& submitted = M5.Display.publishedFrames();
  check(submitted.size() == 1 && submitted[0] == M5.Display.framebuffer(),
        "button navigation submits one complete frame");
  setButtons(true, false, 600);
  check(g_state.sprite_index == pokedex::data::kSpriteCount - 1, "holding left does not repeatedly navigate");
  setButtons(false, false);
  check(g_state.sprite_index == pokedex::data::kSpriteCount - 1, "release after hold does not navigate again");
  setButtons(false, true);
  check(g_state.sprite_index == 0, "right button wraps last to first");
  setButtons(false, false);
  setButtons(false, true);
  check(g_state.sprite_index == 1, "second right press advances exactly once");
  setButtons(false, false);
  check(g_state.sprite_index == 1, "short release does not navigate twice");
  release(233,233,233,233,true);
  check(g_state.reacting, "touch response still works after button navigation");
  setButtons(true, false);
  check(g_state.sprite_index == 0 && !g_state.reacting && g_state.frame_index == 0,
        "button cancels response and starts selected animation");
  setButtons(false, false);
  setButtons(true, true);
  check(g_state.sprite_index == 0, "simultaneous buttons do not choose an arbitrary direction");
  setButtons(false, false);
  M5.Touch.detail.base_x = 300;
  M5.Touch.detail.x = 170;
  M5.Touch.detail.base_y = M5.Touch.detail.y = 233;
  M5.Touch.detail.released = true;
  setButtons(false, true);
  M5.Touch.detail = HostTouchDetail{};
  check(g_state.sprite_index == 1, "button and swipe in same loop only switch once");
  setButtons(false, false);
  // 用真实Button_Class去抖，5ms抖动不能触发第二次切换。
  setButtons(true, false, 5);
  setButtons(false, false, 5);
  check(g_state.sprite_index == 1, "short physical button bounce does not navigate");
  setButtons(true, false, 5);
  setButtons(true, false, 10);
  check(g_state.sprite_index == 0, "stable physical press navigates after debounce");
}

void checkEvolutionSizes() {
  const uint16_t chains[][3] = {{1,2,3}, {4,5,6}, {7,8,9}, {25,26,0},
                                {29,30,31}, {63,64,65}, {129,130,0}};
  for (const auto& chain : chains) {
    size_t previous_area = 0;
    for (const uint16_t id : chain) {
      if (!id) break;
      resetFrame();
      g_state.sprite_index = id - 1;
      check(render(), "evolution comparison renders actual first frame");
      const auto& decoded = pokedex::data::kSprites[id - 1];
      uint16_t scale = pokedex::displayScale(decoded);
      if (decoded.height * scale > 220) scale = 2;
      size_t area = 0;
      for (size_t pixel = 0; pixel < size_t(decoded.width) * decoded.height * scale * scale; ++pixel)
        if (g_pixels[pixel] != pokedex::kTransparentKey) ++area;
      check(area > previous_area, "evolved starter or representative species occupies more visible pixels");
      previous_area = area;
      const auto& s = pokedex::data::kSprites[id - 1];
      std::printf("SIZE #%03u %ux%u visible=%zu\n", unsigned(id),
                  unsigned(s.width * pokedex::displayScale(s)),
                  unsigned(s.height * pokedex::displayScale(s)), area);
      char filename[96];
      std::snprintf(filename, sizeof(filename), "size-%03u.ppm", unsigned(id));
      savePPM(verificationOutput(filename).c_str());
    }
  }
}

void checkViewerReentry() {
  resetFrame();
  begin();
  for (uint16_t selected = 0; selected < 2; ++selected) {
    if (selected) {
      setButtons(false, true);
      setButtons(false, false);
    }
    const auto first = M5.Display.framebuffer();
    const auto& sprite = pokedex::data::kSprites[selected];
    uint32_t elapsed = 0;
    bool changed = false;
    for (uint16_t frame = 1; frame < sprite.frame_count && !changed; ++frame) {
      elapsed += sprite.frames[frame - 1].duration_ms;
      host_time_ms = g_state.animation_started_ms + elapsed;
      loop();
      changed = M5.Display.framebuffer() != first;
    }
    check(changed, "re-entry fixture reaches visibly different animation pixels");
    M5.Display.fillScreen(TFT_BLUE);
    host_time_ms += 1000;
    invalidate();
    M5.Display.beginObservation();
    begin();
    M5.Display.endObservation();
    const auto immediate = M5.Display.framebuffer();
    const auto& published = M5.Display.publishedFrames();
    check(g_state.sprite_index == selected, "re-entry preserves selected species");
    check(published.size() == 1 && published[0] == immediate,
          "re-entry publishes one complete frame");
    check(immediate == first, "first re-entry paint matches the selected species frame zero");
    host_time_ms += 20;
    loop();
    check(M5.Display.framebuffer() == first, "next loop keeps the restarted animation frame");
  }
}

void writeAnimationEvidence() {
  resetFrame();
  g_state.sprite_index = 24;
  render();
  savePPM(verificationOutput("viewer-pikachu.ppm").c_str());
  FILE* raw = std::fopen(verificationOutput("viewer-pikachu.rgb").c_str(), "wb");
  check(raw != nullptr, "open raw animation evidence");
  if (!raw) return;
  const auto& s = pokedex::data::kSprites[24];
  for (uint32_t t = 0; t < 2400; t += 100) {
    if (t == 800) pokedex::viewer::applyInput(g_state, Input::React, pokedex::data::kSpriteCount, t);
    pokedex::viewer::update(g_state, s, t);
    render();
    for (int y=0; y<468; ++y) {
      for (int x=0; x<468; ++x) {
        uint16_t p = M5.Display.readPixel(x,y);
        const unsigned char rgb[] = {static_cast<unsigned char>((p>>11)*255/31),
          static_cast<unsigned char>(((p>>5)&63)*255/63), static_cast<unsigned char>((p&31)*255/31)};
        if (raw) std::fwrite(rgb,1,3,raw);
      }
    }
  }
  if (raw) std::fclose(raw);
  else check(false, "open animation evidence");
}
}

int main() {
  setup();
  if (!g_ready || !M5.Display.getBuffer()) return 2;
  checkSwitchAndResponse();
  checkFullSprites();
  checkLargeAnimation();
  checkInputLoop();
  checkButtonLoop();
  checkViewerReentry();
  checkEvolutionSizes();
  writeAnimationEvidence();
  std::free(g_pixels);
  std::printf("%d render checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
