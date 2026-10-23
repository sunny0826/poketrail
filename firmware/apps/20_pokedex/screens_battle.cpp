#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
void Screens::battle(const ScreenState& s, const Engine& e, const BattleView& practice, uint32_t now) {
  const auto& v = e.view();
  if (s.page == Page::Throw) {
    char text[60];
    ballCount(availableBalls(v), 43);
    line(speciesName(v.encounter.species), 80, TFT_CYAN);
    if (e.throwView().active) {
      sprite(v.encounter.species, now);
      const int32_t remaining = int32_t(e.throwView().targetMs - now);
      const int radius = remaining > 1500 ? 130 : remaining > 0 ? 24 + remaining * 106 / 1500 : 24;
      M5.Display.drawCircle(233, 233, radius, TFT_CYAN);
      M5.Display.drawCircle(233, 233, 24, TFT_YELLOW);
      line("圈到中心 · 右键投球", 391, TFT_CYAN);
      line("未投出也会消耗本球", 419, TFT_DARKGREY);
    } else {
      thumbnail(v.encounter.species, 233, 127, 2);
      snprintf(text, sizeof(text), "剩余 %u 次", 2 - v.encounter.usedBalls);
      line(text, 174, TFT_CYAN);
      if (!availableBalls(v)) line("每 1000 步获得一球", 196, TFT_YELLOW);
      item(s, 0, availableBalls(v) ? "准备投球" : "暂无精灵球");
      item(s, 1, "保留遭遇 · 继续走路");
      item(s, 2, "放弃这次捕捉");
      if (s.command == CommandResult::NoBalls) line(progressionMessage(s.command), 370, TFT_YELLOW);
      line("左键切换 · 右键确认", 391, TFT_CYAN);
      line("轻触选项确认", 419, TFT_DARKGREY);
    }
    return;
  }
  const auto& b = s.page == Page::Practice ? practice : e.battleView();
  uint16_t partner = s.page == Page::Practice ? (v.partner ? v.partner : 1) : v.encounter.partner;
  uint16_t enemy = s.page == Page::Practice ? 25 : v.encounter.species;
  char title[100];
  snprintf(title, sizeof(title), "%s %u  /  %s %u", speciesName(partner), b.playerHp, speciesName(enemy), b.enemyHp);
  line(s.page == Page::Practice ? "免费练习 · 轻触上方暂停" : "轻触上方暂停", 38, TFT_DARKGREY);
  line(title, 62, TFT_WHITE);
  sprite(b.expected == Action::Attack ? partner : enemy, now);
  M5.Display.drawFastHLine(103, 382, 260, TFT_DARKGREY);
  M5.Display.fillRect(221, 376, 24, 13, 0x1946);
  int32_t delta = int32_t(now - b.targetMs);
  int x = 233 + delta * 120 / 1500;
  if (x < 103) x = 103;
  if (x > 363) x = 363;
  M5.Display.fillCircle(x, 382, 5, TFT_CYAN);
  const char* result[] = {"未命中", "命中", "漂亮", "完美"};
  line(b.pauseRequested ? "本拍结束后暂停" : b.consumed ? result[uint8_t(b.lastTiming)] :
       b.expected == Action::Attack ? "右键 · 攻击" : "左键 · 躲避", 411, TFT_YELLOW);
}
} }
