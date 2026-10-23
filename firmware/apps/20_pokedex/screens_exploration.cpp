#include "screens.hpp"
#include "board.hpp"
#include <cstdio>

namespace pokedex { namespace ui {
using namespace adventure;
namespace {
void tree(int x, int y, uint16_t color) {
  M5.Display.fillRect(x - 5, y + 38, 10, 40, 0x3964);
  M5.Display.fillTriangle(x, y, x - 28, y + 42, x + 28, y + 42, color);
  M5.Display.fillTriangle(x, y + 20, x - 36, y + 62, x + 36, y + 62, color);
}
void scene(Route route) {
  M5.Display.setClipRect(5, 98, 456, 270);
  if (route == Route::Forest) {
    M5.Display.fillCircle(233, 233, 218, 0x0924);
    M5.Display.fillEllipse(233, 338, 164, 28, 0x1A26);
    tree(104, 144, 0x1B29); tree(355, 128, 0x1247);
    tree(145, 108, 0x11E5); tree(312, 164, 0x22C8);
    M5.Display.fillRect(112, 341, 24, 4, 0x3327);
    M5.Display.fillRect(328, 324, 20, 4, 0x3327);
    M5.Display.fillRect(146, 360, 16, 4, 0x3327);
  } else if (route == Route::Coast) {
    M5.Display.fillCircle(233, 233, 218, 0x116A);
    M5.Display.fillRect(65, 194, 336, 112, 0x1A4F);
    M5.Display.fillRect(65, 256, 336, 50, 0x2B73);
    M5.Display.fillEllipse(233, 348, 168, 44, 0x8B89);
    for (int x = 85; x < 380; x += 64) {
      M5.Display.fillRect(x, 221, 32, 3, 0x63D7);
      M5.Display.fillRect(x + 16, 286, 24, 3, 0x8CB9);
    }
    M5.Display.fillRect(122, 337, 12, 4, 0xB4CF);
    M5.Display.fillRect(326, 354, 20, 4, 0xB4CF);
  } else {
    M5.Display.fillCircle(233, 233, 218, 0x39C9);
    M5.Display.fillEllipse(233, 246, 156, 140, 0x1083);
    M5.Display.fillTriangle(98, 105, 143, 105, 120, 200, 0x528B);
    M5.Display.fillTriangle(304, 98, 354, 98, 331, 171, 0x528B);
    M5.Display.fillTriangle(170, 98, 190, 98, 182, 140, 0x39C9);
    M5.Display.fillEllipse(233, 352, 150, 24, 0x2946);
    M5.Display.fillTriangle(86, 342, 110, 307, 137, 352, 0x4A6A);
    M5.Display.fillTriangle(310, 351, 345, 316, 376, 350, 0x4A6A);
  }
  M5.Display.clearClipRect();
}
void fittedLine(const char* text, int y, int width, uint16_t color) {
  M5.Display.setFont(&fonts::efontCN_24);
  if (M5.Display.textWidth(text) > width) M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextDatum(middle_center); M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(text, 233, y);
}
}
bool Screens::exploration(const Engine& engine, uint32_t now) {
  const auto& save = engine.view();
  scene(save.route);
  const bool rendered = sprite(save.partner, now, homePlayback_);
  if (!rendered) line("伙伴画面不可用", 233, TFT_RED);
  fittedLine(speciesName(save.partner), 54, 240, TFT_WHITE);
  line(routeName(save.route), 82, TFT_DARKGREY);
  const auto& clock = save.clock;
  const bool todayKnown = clock.trusted && clock.anchor.trusted && !clock.pending.active && !clock.anomaly &&
    save.today.dayKey && save.today.dayKey == clock.anchor.dayKey && save.today.epoch == clock.calibrationEpoch;
  char text[96];
  if (todayKnown) snprintf(text, sizeof(text), "%s %llu 步", save.today.complete ? "今日" : "今日已记录",
                           static_cast<unsigned long long>(save.today.steps));
  else snprintf(text, sizeof(text), "今日 --");
  fittedLine(text, 393, 290, TFT_CYAN);
  if (engine.stepsIncomplete()) line("部分步数缺失", 422, TFT_RED);
  else if (engine.loadResult().degraded) line("已从备用存档恢复", 422, TFT_YELLOW);
  else if (save.phase == Phase::EncounterReady) line("发现宝可梦 · 按键查看", 422, TFT_YELLOW);
  else if (save.phase == Phase::ThrowReady) line("待捕捉 · 按键继续", 422, TFT_YELLOW);
  else line("左 / 右键打开菜单", 422, TFT_DARKGREY);
  return rendered;
}
} }
