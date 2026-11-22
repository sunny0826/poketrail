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
namespace {
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
menu_layout::Rect unionRect(const menu_layout::Rect& a, const menu_layout::Rect& b) {
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
}
void Screens::homeEvolveButton(bool pressed) {
  const auto& b = kHomeEvolveButton;
  if (pressed) {
    panelFrame(b, 12, true, true, theme::kGold);
  } else {
    M5.Display.fillRoundRect(b.x, b.y, b.width, b.height, 12, theme::kPanelActive);
    M5.Display.drawRoundRect(b.x, b.y, b.width, b.height, 12, theme::kGold);
  }
  smallLabel(tr(Text::Evolve), b.x + b.width / 2, b.y + b.height / 2, theme::kGold, middle_center);
}
bool Screens::exploration(const ScreenState& s, const Engine& engine, uint32_t now) {
  const auto& save = engine.view();
  scene(save.route, false, 0, sceneSteps_);
  actorShadow(233, 238);
  uint8_t maxScale = 2;
  if (save.partner) {
    const auto& partner = data::kSprites[save.partner - 1];
    maxScale = displayScale(partner) == 3 && partner.height * 3 > 190 ? 2 : 3;
  }
  const bool rendered = save.partner && sprite(save.partner, now, homePlayback_, 248, 233, 0, maxScale);
  if (!rendered) line(tr(Text::PartnerImageFailed), 233, theme::kRed);
  const bool evolvable = homeEvolutionReady(engine);
  title(speciesName(save.partner), "", theme::routeColor(save.route));
  if (evolvable) homeEvolveButton(s.pressedAux == PressedAux::HomeEvolve);
  M5.Display.fillRoundRect(76, 68, language_ == i18n::Language::English ? 88 : 69, 26, 7, theme::kPanel);
  glyph(theme::Icon::Route, 91, 81, theme::routeColor(save.route));
  smallLabel(routeName(save.route), 107, 81, theme::routeColor(save.route), middle_left);
  captureBadge(save, 318, 81);
  encounterBadge(save, 366, 110);
  progress(save.explorationCredit, save.encounterTarget, 361, theme::routeColor(save.route));
  const auto& clock = save.clock;
  const bool todayKnown = clock.trusted && clock.anchor.trusted && !clock.pending.active && !clock.anomaly &&
    save.today.dayKey && save.today.dayKey == clock.anchor.dayKey && save.today.epoch == clock.calibrationEpoch;
  char text[96];
  if (todayKnown) {
    formatSteps(save.today.steps, text, sizeof(text));
    todayNumber(language_, text, !save.today.complete);
  } else {
    line(tr(Text::TodayUnknown), 393, theme::kText, true);
  }
  if (engine.stepsIncomplete()) line(tr(Text::StepsIncomplete), 428, theme::kRed);
  else if (engine.loadResult().degraded) line(tr(Text::BackupRestored), 428, theme::kGold);
  else if (save.phase == Phase::EncounterReady) line(tr(Text::EncounterMenuHint), 428, theme::kGold);
  else if (save.phase == Phase::ThrowReady) line(tr(Text::CaptureMenuHint), 428, theme::kGold);
  else line(tr(Text::SideKeysMenu), 428, theme::kMuted);
  return rendered;
}
bool Screens::drawHome(const ScreenState& s, const Engine& engine, uint32_t now, bool full) {
  const auto& save = engine.view();
  const bool evolvable = homeEvolutionReady(engine);
  const bool evolvePressed = evolvable && s.pressedAux == PressedAux::HomeEvolve;
  const bool prevEvolvePressed = snap_.homeEvolvable && snap_.pressedAux == PressedAux::HomeEvolve;
  const bool evolveChanged = evolvable != snap_.homeEvolvable || evolvePressed != prevEvolvePressed;
  const uint16_t capturedCount = capturedSpeciesCount(save);
  const uint16_t pending = pendingEncounterCount(save);
  const auto& clock = save.clock;
  const bool todayKnown = clock.trusted && clock.anchor.trusted && !clock.pending.active && !clock.anomaly &&
    save.today.dayKey && save.today.dayKey == clock.anchor.dayKey && save.today.epoch == clock.calibrationEpoch;
  const uint8_t footerKind = computeHomeFooter(engine);
  uint8_t maxScale = 2;
  if (save.partner) {
    const auto& partner = data::kSprites[save.partner - 1];
    maxScale = displayScale(partner) == 3 && partner.height * 3 > 190 ? 2 : 3;
  }
  const menu_layout::Rect spriteBox = spriteBounds(save.partner, 233, 248, maxScale, kMaxDisplayHeight);

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
  auto drawEvolve = [&]() { homeEvolveButton(evolvePressed); };
  auto drawRoutePill = [&]() {
    M5.Display.fillRoundRect(76, 68, language_ == i18n::Language::English ? 88 : 69, 26, 7, theme::kPanel);
    glyph(theme::Icon::Route, 91, 81, theme::routeColor(save.route));
    smallLabel(routeName(save.route), 107, 81, theme::routeColor(save.route), middle_left);
  };
  auto drawToday = [&]() {
    char text[96];
    if (todayKnown) {
      formatSteps(save.today.steps, text, sizeof(text));
      todayNumber(language_, text, !save.today.complete);
    } else {
      line(tr(Text::TodayUnknown), 393, theme::kText, true);
    }
  };
  auto drawFooter = [&]() {
    const Text texts[] = {Text::SideKeysMenu, Text::StepsIncomplete, Text::BackupRestored,
                          Text::EncounterMenuHint, Text::CaptureMenuHint};
    const uint16_t colors[] = {theme::kMuted, theme::kRed, theme::kGold, theme::kGold, theme::kGold};
    line(tr(texts[footerKind]), 428, colors[footerKind]);
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
    const bool headerChanged = save.route != snap_.route || evolveChanged ||
                               capturedCount != snap_.capturedCount;
    const bool pendingChanged = pending != snap_.pendingEncounters;
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
      encounterBadge(save, 366, 110);
      if (headerChanged) {
        clearRegion(74, 66, 320, 26, theme::kBackground);
        if (evolvable) drawEvolve();
        drawRoutePill();
        captureBadge(save, 318, 81);
      } else {
        M5.Display.setClipRect(74, 92, 320, 5);
        if (evolvable) drawEvolve();
        drawRoutePill();
        captureBadge(save, 318, 81);
        M5.Display.clearClipRect();
      }
      clearRegion(133, 362, 212, 4, theme::kBackground);
      progress(save.explorationCredit, save.encounterTarget, 361, theme::routeColor(save.route));
      if (todayChanged) { clearRegion(40, 370, 388, 48, theme::kBackground); drawToday(); }
      if (footerChanged) { clearRegion(40, 418, 388, 20, theme::kBackground); drawFooter(); }
    } else {
      if (headerChanged) {
        WriteScope sc;
        if (evolveChanged) {
          clearRegion(173, 67, 120, 25, theme::kBackground);
          restoreHomeScene(173, 92, 120, 5);
          if (evolvable) drawEvolve();
        }
        if (capturedCount != snap_.capturedCount) {
          clearRegion(302, 66, 92, 26, theme::kBackground);
          restoreHomeScene(302, 92, 92, 5);
          captureBadge(save, 318, 81);
        }
      }
      if (pendingChanged || spriteChanged) {
        WriteScope sc;
        if (pendingChanged) {
          restoreHomeScene(200, 96, 178, 28);
          encounterBadge(save, 366, 110);
        }
        if (spriteChanged) {
          menu_layout::Rect dirty = unionRect(snap_.spriteRect, spriteBox);
          if (!snap_.homePartnerRendered || !save.partner)
            dirty = unionRect(dirty, {60, 220, 346, 28});
          if (dirty.y + dirty.height > 299 && dirty.y < 312)
            dirty = unionRect(dirty, {193, 299, 81, 13});
          restoreHomeScene(dirty.x, dirty.y, dirty.width, dirty.height);
          if (dirty.y + dirty.height > 299 && dirty.y < 312) actorShadow(233, 238);
          rendered = save.partner && sprite(save.partner, now, homePlayback_, 248, 233, 0, maxScale);
          if (!rendered) {
            line(tr(Text::PartnerImageFailed), 233, theme::kRed);
          }
        }
      }
      if (progressChanged || todayChanged || footerChanged) {
        WriteScope sc;
        if (progressChanged) {
          restoreHomeScene(133, 361, 212, 1);
          clearRegion(133, 362, 212, 4, theme::kBackground);
          progress(save.explorationCredit, save.encounterTarget, 361, theme::routeColor(save.route));
        }
        if (todayChanged) { clearRegion(40, 370, 388, 48, theme::kBackground); drawToday(); }
        if (footerChanged) { clearRegion(40, 418, 388, 20, theme::kBackground); drawFooter(); }
      }
    }
  }
  snap_.route = save.route;
  snap_.partner = save.partner;
  snap_.homeEvolvable = evolvable;
  snap_.capturedCount = capturedCount;
  snap_.pendingEncounters = pending;
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
