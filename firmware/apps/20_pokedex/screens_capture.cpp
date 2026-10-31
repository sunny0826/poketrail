#include "screens.hpp"
#include "board.hpp"
#include "icons_generated.hpp"
namespace pokedex { namespace ui {
using namespace adventure;
void Screens::ballAt(int x, int y, float angle) {
  const auto& icon = data::kPokeBallIcon;
  M5.Display.pushImageRotateZoom(float(x), float(y), icon.width / 2.0f, icon.height / 2.0f,
                                 angle, 1.0f, 1.0f, icon.width, icon.height, icon.pixels, kTransparentKey);
}
namespace {
void star(int x, int y, int r, uint16_t color) {
  M5.Display.drawFastHLine(x - r, y, 2 * r + 1, color);
  M5.Display.drawFastVLine(x, y - r, 2 * r + 1, color);
}
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
        line("捕获成功", 399, theme::kGold, true);
      } else {
        const int r = int(90.0f * f.t);
        if (r > 2 && f.t < 1.0f) {
          M5.Display.drawCircle(kBallX, kGroundY, r, TFT_WHITE);
          M5.Display.drawLine(kBallX - r, kGroundY - r / 2, kBallX - r / 2, kGroundY, theme::kGold);
          M5.Display.drawLine(kBallX + r, kGroundY - r / 2, kBallX + r / 2, kGroundY, theme::kGold);
        }
        const int rise = f.t < 0.5f ? int(67 * (1.0f - 2 * f.t)) : 0;
        sprite(species, now, battlePlayback_, 233 + rise, 233, f.t < 0.2f ? TFT_WHITE : 0, 2, 190);
        line("挣脱了", 399, theme::kText, true);
      }
      break;
    case CaptureStage::Done: break;
  }
}
} }
