#include "screens.hpp"
#include "board.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
using menu_layout::Rect;
namespace {
uint8_t homePartnerScale(uint16_t id) {
  if (!id) return 2;
  const auto& p = data::kSprites[id - 1];
  return displayScale(p) == 3 && p.height * 3 > 190 ? 2 : 3;
}
bool homeTodayKnown(const Save& s) {
  const auto& c = s.clock;
  return c.trusted && c.anchor.trusted && !c.pending.active && !c.anomaly &&
         s.today.dayKey && s.today.dayKey == c.anchor.dayKey && s.today.epoch == c.calibrationEpoch;
}
void formatSteps(uint64_t value, char* output, size_t size) {
  char digits[24]; snprintf(digits, sizeof(digits), "%llu", static_cast<unsigned long long>(value));
  const int length = strlen(digits);
  int groups = (length - 1) / 3;
  int out = 0;
  for (int index = 0; index < length && out + 1 < int(size); ++index) {
    output[out++] = digits[index];
    if (groups && index == length - groups * 3 - 1) { output[out++] = ','; --groups; }
  }
  output[out] = '\0';
}
void smallLabel(const char* text, int x, int y, uint16_t color, lgfx::textdatum_t datum) {
  M5.Display.setTextDatum(datum);
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.setTextColor(color);
  M5.Display.drawString(text, x, y);
}
void todayNumber(i18n::Language language, const char* text, bool partial) {
  M5.Display.setFont(&fonts::Font4);
  const int width = M5.Display.textWidth(text);
  if (width > 180 || language == i18n::Language::English) {
    smallLabel(partial ? i18n::text(language, Text::TodayPartialSteps) : i18n::text(language, Text::TodayStepLabel), 233, 380, theme::kMuted, middle_center);
    const lgfx::IFont* numberFont = width > 280 ? static_cast<const lgfx::IFont*>(&fonts::Font0)
                                                : static_cast<const lgfx::IFont*>(&fonts::Font2);
    M5.Display.setFont(numberFont);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(theme::kText);
    M5.Display.drawString(text, 233, 407);
    return;
  }
  smallLabel(partial ? i18n::text(language, Text::TodayPartial) : i18n::text(language, Text::Today), partial ? 139 : 158, 396, theme::kMuted, middle_center);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(theme::kText);
  M5.Display.drawString(text, partial ? 250 : 237, 393);
  smallLabel(i18n::text(language, Text::Steps), partial ? 329 : 316, 397, theme::kMuted, middle_center);
}
Rect unionRect(const Rect& a, const Rect& b) {
  if (a.width <= 0 || a.height <= 0) return b;
  if (b.width <= 0 || b.height <= 0) return a;
  const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
  const int x1 = std::max(a.x + a.width, b.x + b.width);
  const int y1 = std::max(a.y + a.height, b.y + b.height);
  return {x0, y0, x1 - x0, y1 - y0};
}
uint8_t computeHomeFooter(const Engine& engine) {
  const auto& save = engine.view();
  if (engine.stepsIncomplete()) return 1;
  if (engine.loadResult().degraded) return 2;
  if (save.phase == Phase::EncounterReady) return 3;
  if (save.phase == Phase::ThrowReady) return 4;
  return 0;
}
void drawHomeToday(i18n::Language lang, const Save& save, bool todayKnown) {
  if (!todayKnown) {
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::efontCN_24);
    M5.Display.setTextColor(theme::kText);
    M5.Display.drawString(i18n::text(lang, Text::TodayUnknown), 233, 393);
    return;
  }
  char text[96];
  formatSteps(save.today.steps, text, sizeof(text));
  todayNumber(lang, text, !save.today.complete);
}
void drawHomeFooter(i18n::Language lang, uint8_t kind) {
  const Text texts[] = {Text::SideKeysMenu, Text::StepsIncomplete, Text::BackupRestored,
                        Text::EncounterMenuHint, Text::CaptureMenuHint};
  const uint16_t colors[] = {theme::kMuted, theme::kRed, theme::kGold, theme::kGold, theme::kGold};
  smallLabel(i18n::text(lang, texts[kind]), 233, 428, colors[kind], middle_center);
}
}
Rect homeEncounterRect(i18n::Language language, const Save& save) {
  const unsigned count = pendingEncounterCount(save);
  if (!count) return {0, 0, 0, 0};
  char text[32];
  snprintf(text, sizeof(text), i18n::text(language, Text::EncounterCount), count);
  const int w = M5.Display.textWidth(text, &fonts::efontCN_16) + 16;
  return {374 - w, 98, w, 24};
}
Rect homeEncounterTouchArea(i18n::Language language, const Save& save) {
  if (!encounterWaiting(save)) return {0, 0, 0, 0};
  const Rect b = homeEncounterRect(language, save);
  if (b.width <= 0 || b.height <= 0) return {0, 0, 0, 0};
  const int left = b.x - kHomeEvolveTouchPad;
  const int right = b.x + b.width + kHomeEvolveTouchPad;
  const int top = left < kHomeEvolveTouchArea.x + kHomeEvolveTouchArea.width
      ? kHomeEvolveTouchArea.y + kHomeEvolveTouchArea.height
      : std::max(97, b.y - kHomeEvolveTouchPad);
  const int bottom = std::max(b.y + b.height + kHomeEvolveTouchPad, top + 27);
  return {left, top, right - left, bottom - top};
}
TouchHit homeTouchHit(const ScreenState& s, const Engine& e, int x, int y) {
  if (homeEvolutionReady(e) && kHomeEvolveTouchArea.contains(x, y)) return {-1, PressedAux::HomeEvolve};
  if (homeRegionTouchArea(s.language).contains(x, y)) return {-1, PressedAux::HomeRegion};
  if (homeEncounterTouchArea(s.language, e.view()).contains(x, y)) return {-1, PressedAux::HomeEncounter};
  return {-1, PressedAux::None};
}
void Screens::homePill(const Rect& b, int radius, bool active, bool pressed, uint16_t accent, uint16_t bg) {
  if (pressed) {
    panelFrame(b, radius, active, true, accent);
  } else {
    M5.Display.fillRoundRect(b.x, b.y, b.width, b.height, radius, bg);
    M5.Display.drawRoundRect(b.x, b.y, b.width, b.height, radius, accent);
  }
}
void Screens::homeEvolveButton(bool pressed) {
  const Rect& b = kHomeEvolveButton;
  homePill(b, 12, true, pressed, theme::kGold, theme::kPanelActive);
  smallLabel(tr(Text::Evolve), b.x + b.width / 2, b.y + b.height / 2, theme::kGold, middle_center);
}
void Screens::homeRegionPill(Route route, bool pressed) {
  const Rect b = homeRegionRect(language_);
  const uint16_t color = theme::routeColor(route);
  homePill(b, 7, false, pressed, color, theme::kPanel);
  glyph(theme::Icon::Route, b.x + 15, b.y + b.height / 2, color);
  smallLabel(routeName(route), b.x + 31, b.y + b.height / 2, color, middle_left);
}
bool Screens::exploration(const ScreenState& s, const Engine& engine, uint32_t now) {
  const auto& save = engine.view();
  scene(save.route, false, 0, sceneSteps_);
  actorShadow(233, 238);
  const uint8_t maxScale = homePartnerScale(save.partner);
  const bool rendered = save.partner && sprite(save.partner, now, homePlayback_, 248, 233, 0, maxScale);
  if (!rendered) line(tr(Text::PartnerImageFailed), 233, theme::kRed);
  title(speciesName(save.partner), "", theme::routeColor(save.route));
  if (homeEvolutionReady(engine)) homeEvolveButton(s.pressedAux == PressedAux::HomeEvolve);
  homeRegionPill(save.route, s.pressedAux == PressedAux::HomeRegion);
  captureBadge(save, 318, 81);
  encounterBadge(save, 366, 110, encounterWaiting(save) && s.pressedAux == PressedAux::HomeEncounter);
  progress(save.explorationCredit, save.encounterTarget, 361, theme::routeColor(save.route));
  drawHomeToday(language_, save, homeTodayKnown(save));
  drawHomeFooter(language_, computeHomeFooter(engine));
  return rendered;
}
bool Screens::drawHome(const ScreenState& s, const Engine& engine, uint32_t now, bool full) {
  const auto& save = engine.view();
  const bool evolvable = homeEvolutionReady(engine);
  const bool evolvePressed = evolvable && s.pressedAux == PressedAux::HomeEvolve;
  const bool prevEvolvePressed = snap_.homeEvolvable && snap_.pressedAux == PressedAux::HomeEvolve;
  const bool evolveChanged = evolvable != snap_.homeEvolvable || evolvePressed != prevEvolvePressed;
  const bool regionPressed = s.pressedAux == PressedAux::HomeRegion;
  const bool regionPressedChanged = regionPressed != (snap_.pressedAux == PressedAux::HomeRegion);
  const uint16_t capturedCount = capturedSpeciesCount(save);
  const uint16_t pending = pendingEncounterCount(save);
  const bool waiting = encounterWaiting(save);
  const bool encounterPressed = pending > 0 && waiting && s.pressedAux == PressedAux::HomeEncounter;
  const bool prevEncounterPressed =
      snap_.pendingEncounters > 0 && snap_.encounterWaiting && snap_.pressedAux == PressedAux::HomeEncounter;
  const bool encounterChanged =
      pending != snap_.pendingEncounters || waiting != snap_.encounterWaiting ||
      encounterPressed != prevEncounterPressed;
  const bool todayKnown = homeTodayKnown(save);
  const uint8_t footerKind = computeHomeFooter(engine);
  const uint8_t maxScale = homePartnerScale(save.partner);
  const Rect spriteBox = spriteBounds(save.partner, 233, 248, maxScale, kMaxDisplayHeight);

  auto restoreHomeScene = [&](int rx, int ry, int rw, int rh) {
    if (!sceneSteps_) { restoreSceneRegion(rx, ry, rw, rh, save.route, false); return; }
    const int y0 = std::max(ry, 92), y1 = std::min(ry + rh, 362);
    const int x0 = std::max(rx, 0), x1 = std::min(rx + rw, 468);
    if (x0 >= x1 || y0 >= y1) return;
    const unsigned idx = isRegion(save.route) ? regionIndex(save.route) : 0;
    const auto& layers = data::kRegionSceneLayers[idx];
    const unsigned groundTop = layers.groundTop;
    const unsigned groundHeight = data::kSceneHeight - groundTop;
    const unsigned middleOffset = sceneSteps_ % groundHeight;
    const unsigned nearOffset = (sceneSteps_ % groundHeight * 2) % groundHeight;
    const unsigned syStart = unsigned(y0 - 92) / data::kSceneScale;
    const unsigned syEnd = unsigned(y1 - 1 - 92) / data::kSceneScale;
    for (unsigned sy = syStart; sy <= syEnd; ++sy) {
      const uint16_t* row = layers.far.pixels + sy * data::kSceneWidth;
      const bool ground = sy >= groundTop;
      const unsigned middleY = ground ? groundTop + (sy - groundTop + groundHeight - middleOffset) % groundHeight : groundTop;
      const unsigned nearY = ground ? groundTop + (sy - groundTop + groundHeight - nearOffset) % groundHeight : groundTop;
      const uint16_t* middle = layers.middle.pixels + middleY * data::kSceneWidth;
      const uint16_t* near = layers.near.pixels + nearY * data::kSceneWidth;
      for (unsigned sx = 0; sx < data::kSceneWidth; ++sx) {
        uint16_t px = row[sx];
        if (ground && middle[sx] != data::kSceneTransparentKey) px = middle[sx];
        if (ground && near[sx] != data::kSceneTransparentKey) px = near[sx];
        if (!sceneEdgeVisible(sx, sy)) px = theme::kBackground;
        sceneRow_[sx * 2] = sceneRow_[sx * 2 + 1] = px;
      }
      for (unsigned dy = 0; dy < data::kSceneScale; ++dy) {
        const int screenY = 92 + int(sy * data::kSceneScale + dy);
        if (screenY < y0 || screenY >= y1) continue;
        const int v = screenY - c152::kScreenCenter;
        const int span = int(std::sqrt(float(c152::kVisibleRadius * c152::kVisibleRadius - v * v)));
        const int sx0 = std::max(x0, std::max(5, c152::kScreenCenter - span));
        const int sx1 = std::min(x1, std::min(461, c152::kScreenCenter + span + 1));
        if (sx0 < sx1) {
          M5.Display.setClipRect(sx0, screenY, sx1 - sx0, 1);
          M5.Display.pushImage(5, screenY, int(data::kSceneWidth * data::kSceneScale), 1, sceneRow_);
          M5.Display.clearClipRect();
        }
        if (x0 < sx0) clearRegion(x0, screenY, sx0 - x0, 1, theme::kBackground);
        if (sx1 < x1) clearRegion(sx1, screenY, x1 - sx1, 1, theme::kBackground);
      }
    }
  };
  bool rendered = snap_.homePartnerRendered;
  if (full) {
    rendered = exploration(s, engine, now);
  } else {
    if (save.partner) advance(homePlayback_, save.partner, now);
    const uint16_t curFrame = save.partner ? homePlayback_.state.frame_index : UINT16_MAX;
    const bool sceneChanged = save.route != snap_.route || sceneSteps_ != snap_.sceneSteps;
    const bool partnerChanged = save.partner != snap_.partner;
    const bool spriteChanged = partnerChanged || curFrame != snap_.animFrame || !snap_.homePartnerRendered;
    const bool headerChanged = save.route != snap_.route || evolveChanged || regionPressedChanged ||
                               capturedCount != snap_.capturedCount;
    const bool progressChanged = save.explorationCredit != snap_.explorationCredit ||
                                 save.encounterTarget != snap_.encounterTarget ||
                                 save.route != snap_.route;
    const bool todayChanged = todayKnown != snap_.todayKnown ||
                              (todayKnown && (save.today.steps != snap_.todaySteps ||
                                              save.today.complete != snap_.todayComplete));
    const bool footerChanged = footerKind != snap_.homeFooterKind;

    if (partnerChanged) {
      WriteScope sc;
      clearRegion(100, 38, 268, 29, theme::kBackground);
      title(speciesName(save.partner), "", theme::routeColor(save.route));
    }
    if (sceneChanged) {
      WriteScope sc;
      scene(save.route, false, 0, sceneSteps_);
      actorShadow(233, 238);
      rendered = save.partner && sprite(save.partner, now, homePlayback_, 248, 233, 0, maxScale);
      if (!rendered) line(tr(Text::PartnerImageFailed), 233, theme::kRed);
      encounterBadge(save, 366, 110, encounterPressed);
      if (headerChanged) {
        clearRegion(74, 66, 320, 26, theme::kBackground);
        if (evolvable) homeEvolveButton(evolvePressed);
        homeRegionPill(save.route, regionPressed);
        captureBadge(save, 318, 81);
      } else {
        M5.Display.setClipRect(74, 92, 320, 5);
        if (evolvable) homeEvolveButton(evolvePressed);
        homeRegionPill(save.route, regionPressed);
        captureBadge(save, 318, 81);
        M5.Display.clearClipRect();
      }
      clearRegion(133, 362, 212, 4, theme::kBackground);
      progress(save.explorationCredit, save.encounterTarget, 361, theme::routeColor(save.route));
      if (todayChanged) { clearRegion(40, 370, 388, 48, theme::kBackground); drawHomeToday(language_, save, todayKnown); }
      if (footerChanged) { clearRegion(40, 418, 388, 20, theme::kBackground); drawHomeFooter(language_, footerKind); }
    } else {
      if (headerChanged) {
        WriteScope sc;
        if (evolveChanged) {
          clearRegion(173, 67, 120, 25, theme::kBackground);
          restoreHomeScene(173, 92, 120, 5);
          if (evolvable) homeEvolveButton(evolvePressed);
        }
        if (regionPressedChanged) {
          const Rect rb = homeRegionRect(language_);
          clearRegion(rb.x, rb.y, rb.width, 92 - rb.y, theme::kBackground);
          restoreHomeScene(rb.x, 92, rb.width, rb.y + rb.height - 92);
          homeRegionPill(save.route, regionPressed);
        }
        if (capturedCount != snap_.capturedCount) {
          clearRegion(302, 66, 92, 26, theme::kBackground);
          restoreHomeScene(302, 92, 92, 5);
          captureBadge(save, 318, 81);
        }
      }
      if (encounterChanged || spriteChanged) {
        WriteScope sc;
        const bool badgeSpriteOverlap = spriteBox.y < 124 && spriteBox.x < 378 && spriteBox.x + spriteBox.width > 240;
        if (encounterChanged) restoreHomeScene(240, 98, 138, 26);
        if (spriteChanged || (encounterChanged && badgeSpriteOverlap)) {
          Rect dirty = spriteChanged ? unionRect(snap_.spriteRect, spriteBox)
                                     : Rect{240, 98, 138, 26};
          if (spriteChanged) {
            if (!snap_.homePartnerRendered || !save.partner)
              dirty = unionRect(dirty, {60, 220, 346, 28});
            if (dirty.y + dirty.height > 299 && dirty.y < 312)
              dirty = unionRect(dirty, {193, 299, 81, 13});
            restoreHomeScene(dirty.x, dirty.y, dirty.width, dirty.height);
          }
          if (dirty.y + dirty.height > 299 && dirty.y < 312) actorShadow(233, 238);
          rendered = save.partner && sprite(save.partner, now, homePlayback_, 248, 233, 0, maxScale);
          if (!rendered) {
            line(tr(Text::PartnerImageFailed), 233, theme::kRed);
          }
        }
        if (encounterChanged || (spriteChanged && badgeSpriteOverlap))
          encounterBadge(save, 366, 110, encounterPressed);
      }
      if (progressChanged || todayChanged || footerChanged) {
        WriteScope sc;
        if (progressChanged) {
          restoreHomeScene(133, 361, 212, 1);
          clearRegion(133, 362, 212, 4, theme::kBackground);
          progress(save.explorationCredit, save.encounterTarget, 361, theme::routeColor(save.route));
        }
        if (todayChanged) { clearRegion(40, 370, 388, 48, theme::kBackground); drawHomeToday(language_, save, todayKnown); }
        if (footerChanged) { clearRegion(40, 418, 388, 20, theme::kBackground); drawHomeFooter(language_, footerKind); }
      }
    }
  }
  snap_.route = save.route;
  snap_.partner = save.partner;
  snap_.homeEvolvable = evolvable;
  snap_.capturedCount = capturedCount;
  snap_.pendingEncounters = pending;
  snap_.encounterWaiting = waiting;
  snap_.explorationCredit = save.explorationCredit;
  snap_.encounterTarget = save.encounterTarget;
  snap_.todayKnown = todayKnown;
  snap_.todaySteps = save.today.steps;
  snap_.todayComplete = save.today.complete;
  snap_.homeFooterKind = footerKind;
  snap_.sceneSteps = sceneSteps_;
  snap_.spriteRect = spriteBox;
  snap_.animFrame = save.partner ? homePlayback_.state.frame_index : UINT16_MAX;
  snap_.homePartnerRendered = rendered;
  return rendered;
}
} }
