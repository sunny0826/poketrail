#include "screens.hpp"
#include "board.hpp"
#include "battle_presentation.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
namespace {
int pulse(uint32_t age, unsigned start, unsigned peak, unsigned end, int amount) {
  if (age <= start || age >= end) return 0;
  return age < peak ? amount * int(age - start) / int(peak - start) :
                     amount * int(end - age) / int(end - peak);
}
uint8_t actorScale(uint16_t id) {
  const auto& sprite = data::kSprites[id - 1];
  return sprite.width <= 68 && sprite.height <= 65 ? 2 : 1;
}
void brackets(int x, int y, uint16_t color) {
  for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
    const int px = x + sx * 48, py = y + sy * 48;
    M5.Display.drawLine(px, py, px - sx * 10, py, color);
    M5.Display.drawLine(px, py, px, py - sy * 10, color);
  }
}
void impact(int x, int y, uint32_t age, uint16_t color, bool strong) {
  if (age < 100 || age >= 400) return;
  const unsigned count = strong ? 8 : 5;
  const int radius = 12 + int(age - 100) * (strong ? 55 : 40) / 300;
  for (unsigned i = 0; i < count; ++i) {
    const float angle = 6.2831853f * i / count + 0.2f;
    const int px = x + int(std::cos(angle) * radius), py = y + int(std::sin(angle) * radius * 0.65f);
    const int size = strong && i % 2 == 0 ? 4 : 3;
    M5.Display.fillRect(px - size / 2, py - size / 2, size, size, color);
  }
  if (age < 200) {
    const int radius = strong ? 22 : 14;
    M5.Display.fillTriangle(x, y - radius, x + 5, y, x, y + radius, color);
    M5.Display.fillTriangle(x, y - radius, x - 5, y, x, y + radius, color);
    M5.Display.fillTriangle(x - radius, y, x, y - 5, x + radius, y, color);
    M5.Display.fillTriangle(x - radius, y, x, y + 5, x + radius, y, color);
  }
}
}
void Screens::battle(const ScreenState& s, const Engine& e, const BattleView& practice, uint32_t now) {
  const auto& v = e.view();
  if (s.page == Page::Throw) {
    char text[60];
    scene(v.encounter.route, true);
    snprintf(text, sizeof(text), "本场剩余 %u 次", 2 - v.encounter.usedBalls);
    title(speciesName(v.encounter.species), s.capture.active ? "" : e.throwView().active ? "等待投球" :
          availableBalls(v) ? text : "每 1000 步获得一球", theme::kGold);
    ballCount(availableBalls(v), 109, 327);
    if (s.capture.active) {
      capture(s, v.encounter.species, now);
    } else if (e.throwView().active) {
      actorShadow(233, 233);
      sprite(v.encounter.species, now, battlePlayback_, 233, 233, 0, 2, 190);
      const int32_t remaining = int32_t(e.throwView().targetMs - now);
      const int radius = remaining > 1500 ? 130 : remaining > 0 ? 24 + remaining * 106 / 1500 : 24;
      const bool perfect = remaining <= 70 && remaining >= -70;
      M5.Display.drawCircle(233, 233, radius, remaining < 400 ? theme::kGold : theme::kMint);
      M5.Display.drawCircle(233, 233, 24, perfect && (now / 100) % 2 ? theme::kText : theme::kGold);
      const uint8_t charge = s.charging ? captureChargeLevel(now - s.chargeStartMs) : 0;
      if (charge == 1) M5.Display.drawCircle(233, kHandY, 27, theme::kGold);
      else if (charge == 2) M5.Display.drawCircle(233, kHandY, (now / 200) % 2 ? 34 : 30, theme::kGold);
      ballAt(233, kHandY);
      line("按住蓄力 · 松手投球", 399, theme::kMint);
      line("未投出也会消耗本球", 426, theme::kMuted);
    } else {
      actorShadow(233, 147);
      sprite(v.encounter.species, now, battlePlayback_, 147, 233, 0, 2, 110);
      item(s, 0, availableBalls(v) ? "准备投球" : "暂无精灵球", menu_layout::kTop, 0, false, theme::Icon::Practice);
      item(s, 1, "保留遭遇 · 继续走路", menu_layout::kTop, 0, false, theme::Icon::Home);
      item(s, 2, "放弃这次捕捉", menu_layout::kTop, 0, false, theme::Icon::Back);
      if (s.command == CommandResult::NoBalls) line(progressionMessage(s.command), 370, theme::kGold);
      line("左键切换 · 右键确认", 399, theme::kMint);
      line("轻触选项确认", 426, theme::kMuted);
    }
    return;
  }
  const auto& b = s.page == Page::Practice ? practice : e.battleView();
  uint16_t partner = s.page == Page::Practice ? (v.partner ? v.partner : 1) : v.encounter.partner;
  uint16_t enemy = s.page == Page::Practice ? (s.practiceSpecies ? s.practiceSpecies : 25) : v.encounter.species;
  const bool effect = battleEffectActive(b, now);
  const uint32_t age = effect ? now - b.settledMs : kBattleEffectMs;
  const Action action = effect ? battleEffectAction(b) : b.expected;
  const bool attack = action == Action::Attack;
  const bool hit = b.lastTiming != Timing::Miss;
  const bool perfect = effect && b.lastTiming == Timing::Perfect;
  const int shake = perfect && attack && age >= 100 && age < 200 ? ((age / 25) % 2 ? 2 : -2) : 0;
  const int playerX = 155 + shake + (attack ? pulse(age, 0, 100, 400, 34) :
                                     hit ? -pulse(age, 30, 120, 370, 26) : 0);
  const int enemyX = 311 + shake + (attack ? (hit ? pulse(age, 100, 175, 330, 14) : 0) :
                                    -pulse(age, 0, 80, 330, 30));
  const Route route = s.page == Page::Practice ? v.route : v.encounter.route;
  scene(route, true, shake);
  title(s.page == Page::Practice ? "捕获练习" : "节奏对战", "轻触上方暂停", theme::kMint);
  hp(speciesName(partner), b.playerHp, 91, theme::kMint);
  hp(speciesName(enemy), b.enemyHp, 282, theme::kGold);
  const uint8_t playerScale = actorScale(partner), enemyScale = actorScale(enemy);
  if (!attack && hit && age >= 10 && age < 250) {
    sprite(partner, now, battlePlayback_, 232, 145 + shake, theme::kOutline, playerScale);
    sprite(partner, now, battlePlayback_, 232, 133 + shake, theme::kTrack, playerScale);
  }
  if (!attack && age < 100) M5.Display.fillRect(286 - int(age) * 65 / 100, 225, 9, 5, theme::kBlue);
  const int32_t delta = int32_t(now - b.targetMs);
  const bool perfectWindow = delta >= -int32_t(kPerfectWindowMs) && delta <= int32_t(kPerfectWindowMs);
  if (attack && age < 100) brackets(enemyX, 226, perfect || perfectWindow ? theme::kGold : theme::routeColor(route));
  else if (!effect && b.expected == Action::Attack) brackets(enemyX, 226, perfectWindow ? theme::kGold : theme::routeColor(route));
  actorShadow(playerX, 232); actorShadow(enemyX, 226);
  const bool flash = effect && age >= 100 && age < 250 && (age / 50) % 2 == 0;
  sprite(partner, now, battlePlayback_, 232, playerX, flash && !attack && !hit ? theme::kText : 0, playerScale);
  sprite(enemy, now, enemyBattlePlayback_, 226, enemyX, flash && attack && hit ? theme::kText : 0, enemyScale);
  if (effect && attack && hit) impact(enemyX, 222, age, perfect ? theme::kGold : theme::kText, perfect);
  if (effect && !attack && !hit) impact(playerX, 228, age, theme::kRed, false);
  M5.Display.fillRoundRect(99, 344, 268, 27, 8, theme::kBackground);
  M5.Display.drawFastHLine(103, 357, 260, theme::kTrack);
  for (int boundary : {-180, -70, 70, 180}) {
    const int x = 233 + boundary * 120 / int(kBeatIntervalMs);
    M5.Display.drawFastVLine(x, 352, 11, std::abs(boundary) == 70 ? theme::kGold : theme::kMuted);
  }
  M5.Display.drawRoundRect(217, 346, 32, 22, 4, perfectWindow ? theme::kGold : theme::kMint);
  const int x = 233 + std::max(-1625, std::min(1625, int(delta))) * 120 / int(kBeatIntervalMs);
  M5.Display.fillCircle(x, 357, 5, perfect || perfectWindow ? theme::kGold : theme::kMint);
  const char* result[] = {"未命中", "命中", "漂亮", "完美"};
  line(b.pauseRequested ? "本拍结束后暂停" : effect || b.consumed ? result[uint8_t(b.lastTiming)] :
       b.expected == Action::Attack ? "右键 · 攻击" : "左键 · 躲避", 401,
       perfect ? theme::kGold : effect && !hit ? theme::kMuted : theme::kText, true);
  line("每拍只接受第一次动作", 430, theme::kMuted);
}
} }
