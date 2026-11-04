#include <cstdio>
#include <cstdlib>
#include "support/verification_output.hpp"
#include "support/rendered_text.hpp"
#include "../firmware/apps/20_pokedex/viewer_screen.cpp"

uint32_t host_time_ms = 0;
HostSerial Serial;
HostM5 M5;
using namespace pokedex::viewer_screen;
namespace {
int failures = 0, checks = 0;
void check(bool condition, const char* message) {
  ++checks;
  if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}
void resetFrame() {
  M5.Display.fillScreen(TFT_BLACK);
  g_state = State{}; g_screen_drawn = false;
  g_decoded_sprite = g_decoded_frame = g_label_sprite = UINT16_MAX;
  M5.Touch.detail = HostTouchDetail{};
  M5.BtnA = m5::Button_Class{}; M5.BtnB = m5::Button_Class{};
  M5.button_a_down = M5.button_b_down = false;
  g_ready = true; host_time_ms = 0;
}
void compareFreshFrame(const char* message) {
  const auto actual = M5.Display.framebuffer(); const State target = g_state;
  resetFrame(); g_state = target;
  check(render(), "reference frame renders");
  check(actual == M5.Display.framebuffer(), message);
}
void checkSwitchAndResponse() {
  resetFrame(); check(render(), "initial frame renders");
  g_state.frame_index = 5; render();
  compareFreshFrame("animation erases previous frame");
  g_state.sprite_index = 145; g_state.frame_index = 0; render();
  g_state.sprite_index = 24; render();
  compareFreshFrame("species switch clears old sprite and labels");
  pokedex::viewer::applyInput(g_state, Input::React, pokedex::data::kSpriteCount, 0);
  pokedex::viewer::update(g_state, pokedex::data::kSprites[24], 240); render();
  compareFreshFrame("reaction does not leave old sprite behind");
  pokedex::viewer::update(g_state, pokedex::data::kSprites[24], 800); render();
  compareFreshFrame("reaction expiry clears heart and jump");
}
void checkFullSprites() {
  for (size_t i = 0; i < pokedex::data::kSpriteCount; ++i) {
    resetFrame(); g_state.sprite_index = i; g_state.reacting = true; g_state.jump = 14;
    check(render(), "every species renders during its reaction");
    bool inside = true;
    for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x) {
      const int dx = x - 233, dy = y - 233;
      if (M5.Display.readPixel(x, y) && dx * dx + dy * dy >
          c152::kVisibleRadius * c152::kVisibleRadius) inside = false;
    }
    check(inside, "species, labels, heart and hint stay inside the physical circle");
    check(rendered_text::contains(pokedex::data::kSprites[i].types, theme::kBlue),
          "complete species type label survives reaction repaint");
  }
}
void checkLargeAnimation() {
  for (const size_t index : {size_t(4), size_t(5), size_t(41), size_t(147)}) {
    resetFrame(); g_state.sprite_index = index; render();
    const auto& sprite = pokedex::data::kSprites[index];
    pokedex::viewer::applyInput(g_state, Input::React, pokedex::data::kSpriteCount, 0);
    for (uint32_t time : {240u, 480u, 800u}) {
      pokedex::viewer::update(g_state, sprite, time); render();
      compareFreshFrame("large sprite reaction clears its previous position");
    }
    g_state.sprite_index = 3; g_state.frame_index = 0; render();
    compareFreshFrame("switch to a smaller sprite clears the old artwork");
  }
}
void checkLanguageAnimation() {
  resetFrame(); begin(); host_time_ms = 240; update(host_time_ms);
  setLanguage(pokedex::i18n::Language::English);
  check(render(), "English viewer renders");
  check(rendered_text::contains("Bulbasaur", theme::kText) &&
        rendered_text::contains("Grass / Poison", theme::kBlue), "English name and types are readable");
  setLanguage(pokedex::i18n::Language::Chinese); render();
}
}
int main() {
  c152::begin("POKEDEX"); begin();
  if (!g_ready || !M5.Display.getBuffer()) return 2;
  checkSwitchAndResponse(); checkFullSprites(); checkLargeAnimation(); checkLanguageAnimation();
  std::free(g_pixels);
  std::printf("%d render checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
