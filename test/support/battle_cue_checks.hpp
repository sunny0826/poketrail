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
// Outermost approach-ring radius left of the cued actor, or 0 when it has merged into the target ring.
inline int approachRadius(Action action, uint16_t color) {
  const int cx = action == Action::Attack ? kEnemyX : kPartnerX, cy = action == Action::Attack ? kEnemyY : kPartnerY;
  for (int radius = kCueStartRadius; radius > kCueTargetRadius + 2; --radius)
    if (M5.Display.readPixel(cx - radius, cy) == color) return radius;
  return 0;
}
// An arrow note: its tip points to the key and its flat side is empty.
inline bool noteNear(int x, bool attack, uint16_t color, int tolerance = 4) {
  for (int at = x - tolerance; at <= x + tolerance; ++at) {
    const int tip = attack ? at + 6 : at - 6, back = attack ? at - 6 : at + 6;
    if (M5.Display.readPixel(at, kLaneY) == color && M5.Display.readPixel(tip, kLaneY) == color &&
        M5.Display.readPixel(back, kLaneY) != color && M5.Display.readPixel(at, kLaneY - 5) == color)
      return true;
  }
  return false;
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
inline bool pauseGlyph() {
  return M5.Display.readPixel(228, 80) == theme::kMuted && M5.Display.readPixel(237, 80) == theme::kMuted &&
         M5.Display.readPixel(233, 80) != theme::kMuted;
}
}
