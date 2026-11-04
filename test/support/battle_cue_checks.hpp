#pragma once
#include "battle_presentation.hpp"
#include "board.hpp"
#include "screen_theme.hpp"

namespace battle_cue_test {
using pokedex::adventure::Action;
using namespace pokedex::ui;
inline bool ringAround(int cx, int cy, int radius, uint16_t color) {
  return M5.Display.readPixel(cx + radius, cy) == color && M5.Display.readPixel(cx - radius, cy) == color &&
         M5.Display.readPixel(cx, cy + radius) == color && M5.Display.readPixel(cx, cy - radius) == color;
}
inline bool targetRing(Action action, uint16_t color) {
  return action == Action::Attack ? ringAround(kEnemyX, kEnemyY, kCueTargetRadius, color)
                                  : ringAround(kPartnerX, kPartnerY, kCueTargetRadius, color);
}
// The cue circles only the actor whose key it asks for.
inline bool cueOn(Action action, uint16_t color) {
  return targetRing(action, color) && !targetRing(action == Action::Attack ? Action::Dodge : Action::Attack, color);
}
inline bool noCue() {
  for (uint16_t color : {theme::kRed, theme::kBlue, theme::kGold, theme::kText})
    for (Action action : {Action::Attack, Action::Dodge})
      if (targetRing(action, color)) return false;
  return true;
}
inline bool countdownDigitShown(unsigned digit) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  if (!expected.createSprite(80, 60)) return false;
  expected.fillScreen(TFT_BLACK);
  expected.setFont(&fonts::Font6);
  expected.setTextDatum(middle_center);
  expected.setTextColor(theme::kGold);
  const char text[2] = {char('0' + digit), 0};
  expected.drawString(text, 40, 30);
  unsigned ink = 0;
  for (int y = 0; y < 60; ++y) for (int x = 0; x < 80; ++x) {
    const bool want = expected.readPixel(x, y) == theme::kGold;
    const bool got = M5.Display.readPixel(193 + x, kCountdownY - 30 + y) == theme::kGold;
    if (want != got) return false;
    ink += want;
  }
  return ink > 50;
}
}
