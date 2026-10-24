#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
void Screens::practiceThrow(const ScreenState& s, uint32_t now) {
  char text[60];
  line("捕获练习 · 不消耗精灵球", 43, TFT_DARKGREY);
  line(speciesName(s.practiceSpecies), 80, TFT_CYAN);
  if (s.capture.active) { capture(s, s.practiceSpecies, now); return; }
  if (s.practiceThrowing) {
    sprite(s.practiceSpecies, now);
    const int32_t remaining = int32_t(s.practiceTargetMs - now);
    const int radius = remaining > 1500 ? 130 : remaining > 0 ? 24 + remaining * 106 / 1500 : 24;
    const bool perfect = remaining <= 70 && remaining >= -70;
    M5.Display.drawCircle(233, 233, radius, remaining < 400 ? TFT_YELLOW : TFT_CYAN);
    M5.Display.drawCircle(233, 233, 24, perfect && (now / 100) % 2 ? kGold : TFT_YELLOW);
    const uint8_t charge = s.charging ? captureChargeLevel(now - s.chargeStartMs) : 0;
    if (charge == 1) M5.Display.drawCircle(233, kHandY, 27, kGold);
    else if (charge == 2) M5.Display.drawCircle(233, kHandY, (now / 200) % 2 ? 34 : 30, kGold);
    ballAt(233, kHandY);
    line("按住蓄力 · 松手投球", 391, TFT_CYAN);
    line("成功不会收入图鉴", 419, TFT_DARKGREY);
    return;
  }
  thumbnail(s.practiceSpecies, 233, 127, 2);
  snprintf(text, sizeof(text), "剩余 %u 次", 2 - s.practiceUsedBalls);
  line(text, 174, TFT_CYAN);
  line("成功不会收入图鉴", 196, TFT_DARKGREY);
  item(s, 0, "准备投球");
  item(s, 1, "换一只练习");
  item(s, 2, "结束练习");
  line("左键切换 · 右键确认", 391, TFT_CYAN);
  line("轻触选项确认", 419, TFT_DARKGREY);
}
void Screens::practiceResult(const ScreenState& s, uint32_t now) {
  line(s.practiceResult == Result::Captured ? "练习成功" :
       s.practiceResult == Result::Lost ? "对战失败" : "练习结束", 52, TFT_WHITE, true);
  line(speciesName(s.practiceSpecies), 90, TFT_CYAN);
  if (s.practiceResult == Result::Captured) {
    sprite(s.practiceSpecies, now, resultPlayback_, 172);
    line("未收入图鉴", 262, TFT_DARKGREY);
  } else {
    thumbnail(s.practiceSpecies, 233, 160, 2);
    line(s.practiceResult == Result::Lost ? "对战失败 · 下次再试" : "宝可梦逃走了", 240);
  }
  item(s, 0, "再练一只");
  item(s, 1, "返回探索");
}
} }
