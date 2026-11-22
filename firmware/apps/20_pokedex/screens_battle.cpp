#include "screens.hpp"
#include "board.hpp"
#include "battle_presentation.hpp"
#include "encounter_catalog.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
#include <cmath>
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
namespace {
enum class MotionEase : uint8_t { Attack, Dodge, Knockback };
int easePulse(uint32_t age, unsigned start, unsigned peak, unsigned end, int amount, MotionEase mode) {
  if (age <= start || age >= end) return 0;
  if (age < peak) {
    const float u = float(age - start) / float(peak - start);
    const float inv = 1.0f - u;
    const float k = mode == MotionEase::Attack ? (1.0f - inv * inv) : (1.0f - inv * inv * inv);
    return int(amount * k + 0.5f);
  }
  const float v = float(age - peak) / float(end - peak);
  const float inv = 1.0f - v;
  const float k = mode == MotionEase::Attack
                      ? (1.0f - v * v * (3.0f - 2.0f * v))
                      : mode == MotionEase::Dodge
                            ? (inv * inv * (1.0f + 0.45f * std::sin(v * 3.1415926f)))
                            : (inv * inv);
  return std::max(0, std::min(amount, int(amount * k + 0.5f)));
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
void lane(const BattleView& b, uint32_t now) {
  M5.Display.fillRoundRect(233 - kLaneHalfWidth - 10, 344, 2 * kLaneHalfWidth + 21, 27, 13, theme::kBackground);
  M5.Display.drawFastHLine(233 - kLaneHalfWidth, kLaneY, 2 * kLaneHalfWidth + 1, theme::kTrack);
  if (!b.active || b.paused) return;
  const bool next = b.cueIndex + 1 < kMaxCues;
  const uint32_t nextTarget = b.targetMs + (b.enemyHp * 2 < kEnemyHp ? kFrenzyBeatIntervalMs : kBeatIntervalMs);
  if (next && laneVisible(int32_t(nextTarget - now)))
    M5.Display.fillCircle(233 + laneOffset(int32_t(nextTarget - now)), kLaneY, 4, theme::kMuted);
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
void Screens::drawBattle(const ScreenState& s, const Engine& e, const BattleView& practice, uint32_t now, bool full) {
  const auto& v = e.view();
  const auto& b = s.page == Page::Practice ? practice : e.battleView();
  const uint16_t partner = s.page == Page::Practice ? (v.partner ? v.partner : 1) : v.encounter.partner;
  const uint16_t enemy = s.page == Page::Practice ? (s.practiceSpecies ? s.practiceSpecies : 25) : v.encounter.species;
  const bool overrideEffect = s.page == Page::Battle && s.battleEffectOverride &&
                              now - s.battleEffectStartMs < kBattleEffectMs;
  const bool effect = overrideEffect || battleEffectActive(b, now);
  const uint32_t age = effect ? now - (overrideEffect ? s.battleEffectStartMs : b.settledMs) : kBattleEffectMs;
  const Action action = effect ? battleEffectAction(b) : b.expected;
  const bool attack = action == Action::Attack;
  const bool hit = b.lastTiming != Timing::Miss;
  const bool perfect = effect && (b.lastTiming == Timing::Perfect || b.special);
  const int shake = perfect && attack && age >= 100 && age < 200 ? ((age / 25) % 2 ? 2 : -2) : 0;
  const int playerX = kPartnerX + shake + (attack ? easePulse(age, 0, 100, 400, 34, MotionEase::Attack) :
                                           hit ? -easePulse(age, 30, 120, 370, 26, MotionEase::Dodge) : 0);
  const int enemyX = kEnemyX + shake + (attack ? (hit ? easePulse(age, 100, 175, 330, 14, MotionEase::Knockback) : 0) :
                                        -easePulse(age, 0, 80, 330, 30, MotionEase::Attack));
  const Route route = s.page == Page::Practice ? regionOf(enemy) : v.encounter.route;
  const uint8_t playerScale = actorScale(partner), enemyScale = actorScale(enemy);

  auto drawStage = [&]() {
    if (!attack && hit && age >= 10 && age < 250) {
      spriteSilhouette(partner, now, battlePlayback_, kPartnerY, 145 + shake, theme::kOutline, playerScale);
      spriteSilhouette(partner, now, battlePlayback_, kPartnerY, 133 + shake, theme::kTrack, playerScale);
    }
    if (!attack && age < 100) M5.Display.fillRect(286 - int(age) * 65 / 100, 225, 9, 5, theme::kBlue);
    if (attack && age < 100) brackets(enemyX, kEnemyY, perfect ? theme::kGold : theme::routeColor(route));
    actorShadow(playerX, kPartnerY); actorShadow(enemyX, kEnemyY);
    const bool flash = effect && age >= 100 && age < 250 && (age / 50) % 2 == 0;
    if (flash && !attack && !hit)
      spriteSilhouette(partner, now, battlePlayback_, kPartnerY, playerX, theme::kText, playerScale);
    else
      sprite(partner, now, battlePlayback_, kPartnerY, playerX, 0, playerScale);
    if (flash && attack && hit)
      spriteSilhouette(enemy, now, enemyBattlePlayback_, kEnemyY, enemyX, theme::kText, enemyScale);
    else
      sprite(enemy, now, enemyBattlePlayback_, kEnemyY, enemyX, 0, enemyScale);
    if (effect && attack && hit) impact(enemyX, kEnemyY - 4, age, perfect ? theme::kGold : theme::kText, perfect);
    if (effect && !attack && !hit) impact(playerX, kPartnerY - 4, age, theme::kRed, false);
    lane(b, now);
    for (unsigned i = 0; i < kSpecialEnergy; ++i) {
      const bool fullEnergy = b.energy >= kSpecialEnergy;
      const uint16_t color = i < b.energy ? (fullEnergy && (now / 200) % 2 ? theme::kText : theme::kGold) : theme::kDisabled;
      M5.Display.fillCircle(209 + int(i) * 24, 326, 5, color);
    }
    cueRing(b, now);
  };

  const uint8_t countdown = countdownDigit(b, now);
  const char* missText = b.missReason == MissReason::Early ? tr(Text::MissEarly)
                       : b.missReason == MissReason::Late ? tr(Text::MissLate)
                       : b.missReason == MissReason::WrongKey ? tr(Text::MissWrong)
                       : (attack ? tr(Text::Miss) : tr(Text::Hurt));
  const char* result = hit ? tr(attack ? Text::Hit : Text::Dodged) : missText;
  uint8_t footerKind = 0;
  const char* footerText = nullptr;
  uint16_t footerColor = 0;
  if (b.pauseRequested) {
    footerKind = 1; footerText = tr(Text::PauseAfterBeat); footerColor = theme::kText;
  } else if (countdown) {
    footerKind = 2; footerColor = theme::kGold;
  } else if (effect || b.consumed) {
    footerKind = 3;
    footerText = b.special ? tr(Text::SpecialHit) : result;
    footerColor = b.special || perfect ? theme::kGold : effect && !hit ? theme::kMuted : theme::kText;
  } else if (b.frenzy) {
    footerKind = 4; footerText = tr(Text::Frenzy); footerColor = theme::kRed;
  }
  auto drawFooter = [&]() {
    if (footerKind == 2) {
      const char digit[2] = {char('0' + countdown), 0};
      M5.Display.setTextDatum(middle_center);
      M5.Display.setFont(&fonts::Font6);
      M5.Display.setTextColor(theme::kGold);
      M5.Display.drawString(digit, 233, kCountdownY);
    } else if (footerText) {
      line(footerText, 401, footerColor, true);
    }
  };

  if (full) {
    scene(route, true, shake);
    title(s.page == Page::Practice ? tr(Text::CapturePractice) : tr(Text::RhythmBattle), "", theme::kMint);
    pauseGlyph();
    hp(speciesName(partner), b.playerHp, kPlayerHp, false, theme::kMint);
    hp(speciesName(enemy), b.enemyHp, kEnemyHp, true, theme::kGold);
    drawStage();
    drawFooter();
  } else {
    if (shake != 0 || snap_.battleShake != 0 || route != snap_.route) {
      WriteScope sc;
      if (shake != snap_.battleShake) {
        clearRegion(5, 195, 5, 78, theme::kBackground);
        clearRegion(456, 195, 6, 78, theme::kBackground);
      }
      scene(route, true, shake);
      hp(speciesName(partner), b.playerHp, kPlayerHp, false, theme::kMint);
      hp(speciesName(enemy), b.enemyHp, kEnemyHp, true, theme::kGold);
      drawStage();
    } else {
      if (b.playerHp != snap_.playerHp || partner != snap_.partner ||
          b.enemyHp != snap_.enemyHp || enemy != snap_.enemy) {
        WriteScope sc;
        if (b.playerHp != snap_.playerHp || partner != snap_.partner)
          hp(speciesName(partner), b.playerHp, kPlayerHp, false, theme::kMint);
        if (b.enemyHp != snap_.enemyHp || enemy != snap_.enemy)
          hp(speciesName(enemy), b.enemyHp, kEnemyHp, true, theme::kGold);
      }
      const int pHalfW = partner && partner <= data::kSpriteCount ? int(data::kSprites[partner - 1].width) * int(playerScale) / 2 : 0;
      const int eHalfW = enemy && enemy <= data::kSpriteCount ? int(data::kSprites[enemy - 1].width) * int(enemyScale) / 2 : 0;
      const int sx0 = std::min(58, std::min(133, playerX) - pHalfW);
      const int sx1 = std::max(409, enemyX + eHalfW);
      WriteScope sc;
      restoreSceneRegion(sx0, 134, sx1 - sx0, 210, route, true);
      drawStage();
    }
    if (footerKind != snap_.battleFooterKind || countdown != snap_.countdown ||
        footerText != snap_.battleFooterText || footerColor != snap_.battleFooterColor) {
      WriteScope sc;
      clearRegion(40, 374, 388, 56, theme::kBackground);
      drawFooter();
    }
  }
  snap_.route = route;
  snap_.partner = partner;
  snap_.enemy = enemy;
  snap_.playerHp = b.playerHp;
  snap_.enemyHp = b.enemyHp;
  snap_.battleShake = shake;
  snap_.battleFooterKind = footerKind;
  snap_.countdown = countdown;
  snap_.battleFooterText = footerText;
  snap_.battleFooterColor = footerColor;
}
void Screens::drawThrow(const ScreenState& s, const Engine& e, uint32_t now, bool full) {
  const auto& v = e.view();
  const bool legacyNoBalls = v.encounter.ruleVersion < 3 && !availableBalls(v);
  const bool timing = e.throwView().active;
  const int32_t remaining = timing ? int32_t(e.throwView().targetMs - now) : 0;
  if (!full) {
    drawThrowStagePartial(s, v.encounter.route, v.encounter.species, timing, remaining,
                          s.throwCommitting, v.encounter.usedBalls, legacyNoBalls, now, false);
    return;
  }
  battle(s, e, {}, now);
  bool isBurst = false;
  uint8_t promptKind = legacyNoBalls ? 6 : 2;
  if (!legacyNoBalls) {
    if (s.capture.active) {
      const auto f = captureFrameAt(s.capture, now);
      isBurst = f.stage == CaptureStage::Reveal && !s.capture.captured;
      promptKind = f.stage == CaptureStage::Reveal ? (s.capture.captured ? 3 : 4) : 0;
    } else if (timing) {
      promptKind = s.throwCommitting ? 0 : 1;
    } else if (throwCancelNotice(s, now)) {
      promptKind = 7;
    }
  }
  snap_.route = v.encounter.route;
  snap_.species = v.encounter.species;
  snap_.legacyNoBalls = legacyNoBalls;
  snap_.showBallSlots = !s.capture.active && !legacyNoBalls;
  snap_.usedBalls = v.encounter.usedBalls;
  snap_.captureActive = s.capture.active;
  snap_.prevCaptureBurst = isBurst;
  snap_.throwPromptKind = promptKind;
  snap_.timingActive = timing;
  snap_.throwCommitting = s.throwCommitting;
  snap_.spriteRect = spriteBounds(v.encounter.species, 233, legacyNoBalls ? 147 : 233, 2, legacyNoBalls ? 110 : 190);
  snap_.animFrame = v.encounter.species ? battlePlayback_.state.frame_index : UINT16_MAX;
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
      aim(v.encounter.species, now, true, int32_t(e.throwView().targetMs - now), s.throwCommitting);
      if (!s.throwCommitting) line(tr(Text::ReleaseThrowCost), 399, theme::kMint);
    } else if (legacyNoBalls) {
      actorShadow(233, 147);
      sprite(v.encounter.species, now, battlePlayback_, 147, 233, 0, 2, 110);
      line(tr(Text::NoBalls), 196, theme::kRed);
      item(s, 0, tr(Text::EndEncounter), menu_layout::kTop);
    } else {
      aim(v.encounter.species, now, false, 0);
      line(tr(Text::HoldThrow), 399, throwCancelNotice(s, now) ? theme::kGold : theme::kMint);
    }
    return;
  }
  drawBattle(s, e, practice, now, true);
}
} }
