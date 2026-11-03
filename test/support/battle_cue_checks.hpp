#pragma once
#include "battle_presentation.hpp"
#include "board.hpp"
#include "screen_theme.hpp"
#include "rendered_text.hpp"

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
// A round, neutral marker has no tip or action colour. Allow one Runtime frame of movement.
inline bool previewDotNear(int x, int tolerance = 4) {
  for (int at = x - tolerance; at <= x + tolerance; ++at) {
    bool round = true;
    for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx) {
      const bool ink = M5.Display.readPixel(at + dx, kLaneY + dy) == theme::kMuted;
      if (ink != (M5.Display.readPixel(at - dx, kLaneY + dy) == theme::kMuted) ||
          ink != (M5.Display.readPixel(at + dx, kLaneY - dy) == theme::kMuted)) round = false;
    }
    if (round && ringAround(at, kLaneY, 4, theme::kMuted) &&
        M5.Display.readPixel(at, kLaneY) == theme::kMuted &&
        M5.Display.readPixel(at + 4, kLaneY + 4) != theme::kMuted) return true;
  }
  return false;
}
// Scan the lane for extra markers, direction colours, timing boxes and window ticks.
inline bool nextOnlyLane(const pokedex::adventure::BattleView& b, uint32_t now) {
  using namespace pokedex::adventure;
  const int32_t next = int32_t(b.targetMs + (b.enemyHp * 2 < kEnemyHp ? kFrenzyBeatIntervalMs : kBeatIntervalMs) - now);
  const bool visible = b.active && !b.paused && b.cueIndex + 1 < kMaxCues && laneVisible(next);
  const int x = 233 + laneOffset(next);
  if (visible && !previewDotNear(x)) return false;
  unsigned runs = 0; bool inDot = false;
  for (int px = 68; px <= 398; ++px) {
    const bool dot = M5.Display.readPixel(px, kLaneY) == theme::kMuted;
    if (dot && !inDot) ++runs;
    inDot = dot;
    for (int py = 350; py <= 364; ++py) {
      const uint16_t ink = M5.Display.readPixel(px, py);
      if (ink == theme::kGold || ink == theme::kMint || ink == theme::kRed || ink == theme::kBlue || ink == theme::kText) return false;
      if (ink == theme::kMuted && (!visible || px < x - 8 || px > x + 8 || py < kLaneY - 4 || py > kLaneY + 4)) return false;
    }
  }
  return runs == unsigned(visible);
}
inline bool noActionPrompt() {
  for (const char* text : {"右键 · 攻击", "左键 · 躲避", "右键攻击 · 双键必杀",
                           "Right · Attack", "Left · Dodge", "Right: attack · Both: special"})
    for (uint16_t color : {theme::kRed, theme::kBlue})
      if (rendered_text::contains(text, color, 385, 418)) return false;
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
inline bool pauseGlyph() {
  return M5.Display.readPixel(228, 80) == theme::kMuted && M5.Display.readPixel(237, 80) == theme::kMuted &&
         M5.Display.readPixel(233, 80) != theme::kMuted;
}
}
