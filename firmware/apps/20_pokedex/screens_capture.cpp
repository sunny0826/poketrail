#include "screens.hpp"
#include "board.hpp"
#include "icons_generated.hpp"
#include <algorithm>
#include <cmath>
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
void Screens::ballAt(int x, int y, float angle) {
  const auto& icon = data::kPokeBallIcon;
  if (std::fabs(angle) < 0.01f) {
    M5.Display.pushImage(x - icon.width / 2, y - icon.height / 2,
                         icon.width, icon.height, icon.pixels, kTransparentKey);
    return;
  }
  M5.Display.pushImageRotateZoom(float(x), float(y), icon.width / 2.0f, icon.height / 2.0f,
                                 angle, 1.0f, 1.0f, icon.width, icon.height, icon.pixels, kTransparentKey);
}
namespace {
void star(int x, int y, int r, uint16_t color) {
  M5.Display.drawFastHLine(x - r, y, 2 * r + 1, color);
  M5.Display.drawFastVLine(x, y - r, 2 * r + 1, color);
}
menu_layout::Rect unionRect(const menu_layout::Rect& a, const menu_layout::Rect& b) {
  if (a.width <= 0 || a.height <= 0) return b;
  if (b.width <= 0 || b.height <= 0) return a;
  const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
  const int x1 = std::max(a.x + a.width, b.x + b.width);
  const int y1 = std::max(a.y + a.height, b.y + b.height);
  return {x0, y0, x1 - x0, y1 - y0};
}
}
void Screens::aim(uint16_t species, uint32_t now, bool timing, int32_t remaining, bool committing) {
  actorShadow(233, 233);
  sprite(species, now, battlePlayback_, 233, 233, 0, 2, 190);
  if (timing) {
    const bool overcharged = remaining < 0;
    const int radius = overcharged
                           ? std::max(16, 20 + int(remaining) * 4 / 180)
                           : (remaining > 1500 ? 130 : remaining > 0 ? 24 + remaining * 106 / 1500 : 24);
    const bool perfect = remaining <= 70 && remaining >= -70;
    const uint16_t ring = overcharged ? theme::kRed : (remaining < 400 ? theme::kGold : theme::kMint);
    const uint16_t target = overcharged ? theme::kRed : (perfect && (now / 100) % 2 ? theme::kText : theme::kGold);
    M5.Display.drawCircle(233, 233, radius, ring);
    M5.Display.drawCircle(233, 233, radius - 1, ring);
    M5.Display.drawCircle(233, 233, 24, target);
    M5.Display.drawCircle(233, 233, 25, target);
  }
  const int ballY = committing ? kHandY - 10 : kHandY;
  if (committing) {
    M5.Display.drawCircle(kBallX, ballY, 18, theme::kGold);
    M5.Display.drawCircle(kBallX, ballY, 17, theme::kGold);
  }
  ballAt(kBallX, ballY);
}
void Screens::capture(const ScreenState& s, uint16_t species, uint32_t now) {
  const auto& anim = s.capture;
  const auto f = captureFrameAt(anim, now);
  switch (f.stage) {
    case CaptureStage::Flight: {
      sprite(species, now, battlePlayback_, 233, 233, 0, 2, 190);
      int x, y;
      captureFlightPos(f.t, x, y);
      const unsigned trail = anim.timing == Timing::Perfect ? 3 : anim.timing == Timing::Nice ? 2 : 0;
      for (unsigned i = 1; i <= trail; ++i) {
        int tx, ty;
        captureFlightPos(f.t - 0.06f * i, tx, ty);
        M5.Display.fillCircle(tx, ty, 6 - int(i), anim.timing == Timing::Perfect ? theme::kGold : TFT_WHITE);
      }
      ballAt(x, y, 720.0f * f.t);
      break;
    }
    case CaptureStage::Flash: {
      if (f.t < 0.65f)
        sprite(species, now, battlePlayback_, 233, 233, TFT_WHITE, 2, 190);
      ballAt(kBallX, 233);
      const int r = int(90.0f * f.t);
      if (r > 2) {
        M5.Display.drawCircle(233, 233, r, TFT_WHITE);
        M5.Display.drawCircle(233, 233, r / 2, TFT_WHITE);
      }
      break;
    }
    case CaptureStage::Drop:
      M5.Display.fillEllipse(kBallX, kGroundY + 14, 27, 5, theme::kShadow);
      if (f.t >= 0.25f && f.t < 0.6f) {
        const int radius = 20 + int((f.t - 0.25f) * 100);
        M5.Display.drawEllipse(kBallX, kGroundY + 14, radius, 7, theme::kGold);
      }
      ballAt(kBallX, captureDropY(f.t));
      break;
    case CaptureStage::Wiggle: {
      M5.Display.fillEllipse(kBallX, kGroundY + 14, 27, 5, theme::kShadow);
      ballAt(kBallX + int(captureWiggleAngle(f.t) / 4), kGroundY, captureWiggleAngle(f.t));
      const int side = f.wiggle % 2 ? 1 : -1;
      star(233 + side * 40, 285, 5, theme::kGold);
      break;
    }
    case CaptureStage::Reveal:
      if (anim.captured) {
        for (unsigned i = 0; i < 6; ++i) {
          int x, y;
          captureStarPos(i, f.t, x, y);
          star(x, y, 7, i % 2 ? theme::kGold : TFT_WHITE);
        }
        M5.Display.drawCircle(233, kGroundY, 26, theme::kGold);
        ballAt(kBallX, kGroundY);
        line(tr(Text::CatchSuccess), 399, theme::kGold, true);
      } else {
        const int r = int(90.0f * f.t);
        if (r > 2 && f.t < 1.0f) {
          M5.Display.drawCircle(kBallX, kGroundY, r, TFT_WHITE);
          M5.Display.drawLine(kBallX - r, kGroundY - r / 2, kBallX - r / 2, kGroundY, theme::kGold);
          M5.Display.drawLine(kBallX + r, kGroundY - r / 2, kBallX + r / 2, kGroundY, theme::kGold);
        }
        const int rise = f.t < 0.5f ? int(67 * (1.0f - 2 * f.t)) : 0;
        sprite(species, now, battlePlayback_, 233 + rise, 233, f.t < 0.2f ? TFT_WHITE : 0, 2, 190);
        line(tr(Text::BrokeFree), 399, theme::kText, true);
      }
      break;
    case CaptureStage::Done: break;
  }
}
void Screens::drawThrowPrompt(uint8_t promptKind) {
  if (promptKind == 1) {
    line(tr(Text::ReleaseThrowCost), 399, theme::kMint);
  } else if (promptKind == 2 || promptKind == 5 || promptKind == 7 || promptKind == 8) {
    line(tr(Text::HoldThrow), 399, (promptKind == 7 || promptKind == 8) ? theme::kGold : theme::kMint);
    if (promptKind == 5 || promptKind == 8) line(tr(Text::EndPracticeTop), 426, theme::kMuted);
  }
}
void Screens::drawThrowStagePartial(const ScreenState& s, Route route, uint16_t species,
                                    bool timing, int32_t remaining, bool committing,
                                    uint8_t usedBalls, bool legacyNoBalls, uint32_t now,
                                    bool allowExitHint) {
  const bool showSlots = !s.capture.active && !legacyNoBalls;
  bool isReveal = false, isBurst = false;
  uint8_t promptKind = 0;
  if (legacyNoBalls) {
    promptKind = 6;
  } else if (s.capture.active) {
    const auto f = captureFrameAt(s.capture, now);
    isReveal = f.stage == CaptureStage::Reveal;
    isBurst = isReveal && !s.capture.captured;
    if (isReveal) promptKind = s.capture.captured ? 3 : 4;
  } else if (timing) {
    promptKind = committing ? 0 : 1;
  } else {
    const bool notice = throwCancelNotice(s, now);
    promptKind = allowExitHint ? (notice ? 8 : 5) : (notice ? 7 : 2);
  }
  const int cy = legacyNoBalls ? 147 : 233;
  const uint16_t maxH = legacyNoBalls ? 110 : 190;
  const auto spriteBox = spriteBounds(species, 233, cy, 2, maxH);

  if (legacyNoBalls != snap_.legacyNoBalls || route != snap_.route || species != snap_.species) {
    WriteScope sc;
    clearRegion(0, 36, 468, 56, theme::kBackground);
    scene(route, true);
    clearRegion(0, 362, 468, 78, theme::kBackground);
    title(speciesName(species), "", theme::kGold);
    if (legacyNoBalls) {
      actorShadow(233, 147);
      sprite(species, now, battlePlayback_, 147, 233, 0, 2, 110);
      line(tr(Text::NoBalls), 196, theme::kRed);
      item(s, 0, tr(Text::EndEncounter), menu_layout::kTop);
    } else {
      if (showSlots) ballSlots(usedBalls);
      if (s.capture.active) capture(s, species, now);
      else {
        aim(species, now, timing, remaining, committing);
        drawThrowPrompt(promptKind);
      }
    }
  } else if (legacyNoBalls) {
    if (species) advance(battlePlayback_, species, now);
    const uint16_t curFrame = species ? battlePlayback_.state.frame_index : UINT16_MAX;
    if (curFrame != snap_.animFrame) {
      auto dirty = unionRect(snap_.spriteRect, spriteBox);
      dirty = unionRect(dirty, {60, 186, 348, 22});
      if (dirty.y + dirty.height > 208) dirty.height = 208 - dirty.y;
      if (dirty.height > 0) {
        WriteScope sc;
        restoreSceneRegion(dirty.x, dirty.y, dirty.width, dirty.height, route, true);
        M5.Display.setClipRect(dirty.x, dirty.y, dirty.width, dirty.height);
        sprite(species, now, battlePlayback_, 147, 233, 0, 2, 110);
        M5.Display.clearClipRect();
        line(tr(Text::NoBalls), 196, theme::kRed);
      }
    }
  } else {
    if (showSlots != snap_.showBallSlots || (showSlots && usedBalls != snap_.usedBalls)) {
      WriteScope sc;
      restoreBackdrop(195, 67, 76, 30, route, true, true);
      if (showSlots) ballSlots(usedBalls);
    }
    const bool pureIdle = !s.capture.active && !timing && !snap_.captureActive &&
                          !snap_.timingActive && snap_.throwPromptKind == promptKind &&
                          !snap_.prevCaptureBurst;
    if (pureIdle) {
      if (species) advance(battlePlayback_, species, now);
      const uint16_t curFrame = species ? battlePlayback_.state.frame_index : UINT16_MAX;
      if (curFrame != snap_.animFrame) {
        auto dirty = unionRect(snap_.spriteRect, spriteBox);
        if (dirty.y + dirty.height > 294 && dirty.y < 307)
          dirty = unionRect(dirty, {193, 294, 81, 13});
        if (dirty.y + dirty.height > 329) dirty.height = 329 - dirty.y;
        if (dirty.height > 0) {
          WriteScope sc;
          restoreSceneRegion(dirty.x, dirty.y, dirty.width, dirty.height, route, true);
          if (dirty.y + dirty.height > 294 && dirty.y < 307) actorShadow(233, 233);
          sprite(species, now, battlePlayback_, 233, 233, 0, 2, 190);
        }
      }
    } else {
      const int sx0 = std::min(102, spriteBox.x);
      const int sx1 = std::max(364, spriteBox.x + spriteBox.width);
      const int burstBottom = std::max(390, 300 + spriteBox.height / 2 + 1);
      const int stageBottom = (isBurst || snap_.prevCaptureBurst) ? burstBottom : 368;
      const bool promptDirty = promptKind != snap_.throwPromptKind || stageBottom > 368;
      WriteScope sc;
      if (promptDirty) clearRegion(60, 384, 348, 54, theme::kBackground);
      restoreBackdrop(sx0, 103, sx1 - sx0, stageBottom - 103, route, true, true);
      if (s.capture.active) {
        if (isReveal && !promptDirty) {
          M5.Display.setClipRect(0, 0, 468, stageBottom);
          capture(s, species, now);
          M5.Display.clearClipRect();
        } else {
          capture(s, species, now);
        }
      } else {
        aim(species, now, timing, remaining, committing);
        if (promptDirty) drawThrowPrompt(promptKind);
      }
    }
  }
  snap_.route = route;
  snap_.species = species;
  snap_.legacyNoBalls = legacyNoBalls;
  snap_.showBallSlots = showSlots;
  snap_.usedBalls = usedBalls;
  snap_.captureActive = s.capture.active;
  snap_.prevCaptureBurst = isBurst;
  snap_.throwPromptKind = promptKind;
  snap_.timingActive = timing;
  snap_.throwCommitting = committing;
  snap_.spriteRect = spriteBox;
  snap_.animFrame = species ? battlePlayback_.state.frame_index : UINT16_MAX;
}
} }
