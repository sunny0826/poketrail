#include "screens.hpp"
#include "board.hpp"
#include "battle_presentation.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
#include <cmath>
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
uint16_t cueColor(bool attack, bool blink) { return blink ? theme::kText : attack ? theme::kRed : theme::kBlue; }
// The arrow points at the key: right for attack, left for dodge.
void note(int x, bool attack, uint16_t color) {
  if (attack) M5.Display.fillTriangle(x - 5, kLaneY - 7, x - 5, kLaneY + 7, x + 7, kLaneY, color);
  else M5.Display.fillTriangle(x + 5, kLaneY - 7, x + 5, kLaneY + 7, x - 7, kLaneY, color);
}
void lane(const BattleView& b, uint32_t now) {
  const int32_t normalMs = b.frenzy ? kFrenzyNormalWindowMs : kNormalWindowMs;
  const int32_t perfectMs = b.frenzy ? kFrenzyPerfectWindowMs : kPerfectWindowMs;
  const int32_t remaining = int32_t(b.targetMs - now);
  const bool perfectWindow = remaining >= -perfectMs && remaining <= perfectMs;
  M5.Display.fillRoundRect(233 - kLaneHalfWidth - 10, 344, 2 * kLaneHalfWidth + 21, 27, 13, theme::kBackground);
  M5.Display.drawFastHLine(233 - kLaneHalfWidth, kLaneY, 2 * kLaneHalfWidth + 1, theme::kTrack);
  for (int32_t boundary : {-normalMs, -perfectMs, perfectMs, normalMs})
    M5.Display.drawFastVLine(233 + laneOffset(boundary), 352, 11, std::abs(boundary) == perfectMs ? theme::kGold : theme::kMuted);
  const int box = laneOffset(normalMs) + 3;
  M5.Display.drawRoundRect(233 - box, 346, 2 * box + 1, 22, 4, perfectWindow ? theme::kGold : theme::kMint);
  if (!b.active || b.paused) return;
  const bool next = b.cueIndex + 1 < kMaxCues;
  const uint32_t nextTarget = b.targetMs + (b.enemyHp * 2 < kEnemyHp ? kFrenzyBeatIntervalMs : kBeatIntervalMs);
  if (next && laneVisible(int32_t(nextTarget - now)))
    note(233 + laneOffset(int32_t(nextTarget - now)), shownNextAction(b) == Action::Attack,
         cueColor(shownNextAction(b) == Action::Attack, false));
  if (!b.consumed && laneVisible(remaining)) {
    const bool attack = shownAction(b, now) == Action::Attack;
    note(233 + laneOffset(remaining), attack,
         perfectWindow ? theme::kGold : cueColor(attack, b.frenzy && (now / 90) % 2));
  }
}
void pauseGlyph() {
  M5.Display.fillRect(226, 73, 5, 15, theme::kMuted);
  M5.Display.fillRect(235, 73, 5, 15, theme::kMuted);
}
}
void Screens::cueRing(const BattleView& b, uint32_t now) {
  if (!b.active || b.paused || b.consumed) return;
  const int32_t remaining = int32_t(b.targetMs - now);
  const int32_t normalMs = b.frenzy ? kFrenzyNormalWindowMs : kNormalWindowMs;
  if (remaining > int32_t(kCueLeadMs) || remaining < -normalMs) return;
  const bool attack = shownAction(b, now) == Action::Attack;
  const int cx = attack ? kEnemyX : kPartnerX, cy = attack ? kEnemyY : kPartnerY;
  const uint16_t color = cueColor(attack, b.frenzy && (now / 90) % 2);
  const int32_t perfectMs = b.frenzy ? kFrenzyPerfectWindowMs : kPerfectWindowMs;
  const bool perfect = remaining >= -perfectMs && remaining <= perfectMs;
  for (int i = 0; i < 2; ++i) M5.Display.drawCircle(cx, cy, kCueTargetRadius - i, perfect ? theme::kGold : color);
  const int radius = cueRadius(remaining);
  if (radius > kCueTargetRadius + 2)
    for (int i = 0; i < 3; ++i) M5.Display.drawCircle(cx, cy, radius - i, color);
}
void Screens::ballSlots(uint8_t used) {
  for (unsigned i = 0; i < 2; ++i) {
    if (i >= used) { ballAt(ballSlotX(i), kBallSlotY); continue; }
    M5.Display.drawCircle(ballSlotX(i), kBallSlotY, 13, theme::kDisabled);
    M5.Display.drawCircle(ballSlotX(i), kBallSlotY, 12, theme::kDisabled);
  }
}
void Screens::battle(const ScreenState& s, const Engine& e, const BattleView& practice, uint32_t now) {
  const auto& v = e.view();
  if (s.page == Page::Throw) {
    scene(v.encounter.route, true);
    const bool legacyNoBalls = v.encounter.ruleVersion < 3 && !availableBalls(v);
    title(speciesName(v.encounter.species), "", theme::kGold);
    if (!s.capture.active && !legacyNoBalls) ballSlots(v.encounter.usedBalls);
    if (s.capture.active) {
      capture(s, v.encounter.species, now);
    } else if (e.throwView().active) {
      aim(v.encounter.species, now, true, int32_t(e.throwView().targetMs - now), s.charging, now - s.chargeStartMs);
      line("松手投球 · 未投出也计一次", 399, theme::kMint);
    } else if (legacyNoBalls) {
      // A legacy encounter with an empty ball stock cannot be thrown, and the
      // menu is gone, so the page carries its own way out.
      actorShadow(233, 147);
      sprite(v.encounter.species, now, battlePlayback_, 147, 233, 0, 2, 110);
      line("本场没有可用精灵球", 196, theme::kRed);
      item(s, 0, "结束本场 · 返回探索", menu_layout::kTop);
    } else {
      // Aiming state: the ball waits in the hand, no window runs yet and no
      // ring is drawn, so the ring only ever means "the clock is running".
      aim(v.encounter.species, now, false, 0, false, 0);
      line("按住右键蓄力 · 松手投球", 399, theme::kMint);
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
  const bool perfect = effect && (b.lastTiming == Timing::Perfect || b.special);
  const int shake = perfect && attack && age >= 100 && age < 200 ? ((age / 25) % 2 ? 2 : -2) : 0;
  const int playerX = kPartnerX + shake + (attack ? pulse(age, 0, 100, 400, 34) :
                                           hit ? -pulse(age, 30, 120, 370, 26) : 0);
  const int enemyX = kEnemyX + shake + (attack ? (hit ? pulse(age, 100, 175, 330, 14) : 0) :
                                        -pulse(age, 0, 80, 330, 30));
  const Route route = s.page == Page::Practice ? v.route : v.encounter.route;
  scene(route, true, shake);
  title(s.page == Page::Practice ? "捕获练习" : "节奏对战", "", theme::kMint);
  pauseGlyph();
  hp(speciesName(partner), b.playerHp, kPlayerHp, false, theme::kMint);
  hp(speciesName(enemy), b.enemyHp, kEnemyHp, true, theme::kGold);
  const uint8_t playerScale = actorScale(partner), enemyScale = actorScale(enemy);
  if (!attack && hit && age >= 10 && age < 250) {
    sprite(partner, now, battlePlayback_, kPartnerY, 145 + shake, theme::kOutline, playerScale);
    sprite(partner, now, battlePlayback_, kPartnerY, 133 + shake, theme::kTrack, playerScale);
  }
  if (!attack && age < 100) M5.Display.fillRect(286 - int(age) * 65 / 100, 225, 9, 5, theme::kBlue);
  if (attack && age < 100) brackets(enemyX, kEnemyY, perfect ? theme::kGold : theme::routeColor(route));
  actorShadow(playerX, kPartnerY); actorShadow(enemyX, kEnemyY);
  const bool flash = effect && age >= 100 && age < 250 && (age / 50) % 2 == 0;
  sprite(partner, now, battlePlayback_, kPartnerY, playerX, flash && !attack && !hit ? theme::kText : 0, playerScale);
  sprite(enemy, now, enemyBattlePlayback_, kEnemyY, enemyX, flash && attack && hit ? theme::kText : 0, enemyScale);
  if (effect && attack && hit) impact(enemyX, kEnemyY - 4, age, perfect ? theme::kGold : theme::kText, perfect);
  if (effect && !attack && !hit) impact(playerX, kPartnerY - 4, age, theme::kRed, false);
  lane(b, now);
  for (unsigned i = 0; i < kSpecialEnergy; ++i) {
    const bool full = b.energy >= kSpecialEnergy;
    const uint16_t color = i < b.energy ? (full && (now / 200) % 2 ? theme::kText : theme::kGold) : theme::kDisabled;
    M5.Display.fillCircle(209 + int(i) * 24, 326, 5, color);
  }
  cueRing(b, now);
  const uint8_t countdown = countdownDigit(b, now);
  const char* result = attack ? (hit ? "命中" : "未命中") : (hit ? "躲开了" : "受伤了");
  const bool cueVisible = int32_t(b.targetMs - now) <= int32_t(kCueLeadMs);
  const bool shownAttack = shownAction(b, now) == Action::Attack;
  const char* prompt = !cueVisible ? (b.frenzy ? "狂暴" : "") : shownAttack ?
    (b.energy >= kSpecialEnergy ? "右键攻击 · 双键必杀" : "右键 · 攻击") : "左键 · 躲避";
  const uint16_t promptColor = !cueVisible ? theme::kRed : shownAttack ? theme::kRed : theme::kBlue;
  if (b.pauseRequested) line("本拍结束后暂停", 401, theme::kText, true);
  else if (countdown) {
    const char digit[2] = {char('0' + countdown), 0};
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::Font6);
    M5.Display.setTextColor(theme::kGold);
    M5.Display.drawString(digit, 233, kCountdownY);
  } else if (effect || b.consumed)
    line(b.special ? "必杀命中" : result, 401,
         b.special || perfect ? theme::kGold : effect && !hit ? theme::kMuted : theme::kText, true);
  else if (*prompt) line(prompt, 401, promptColor, true);
}
} }
