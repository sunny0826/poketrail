#include "screens.hpp"
#include "board.hpp"
#include "sprites_generated.hpp"
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
}
bool Screens::exploration(const Engine& engine, uint32_t now) {
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
  title(speciesName(save.partner), evolvable ? "" : tr(Text::Carrying), theme::routeColor(save.route));
  if (evolvable) {
    const auto& b = kHomeEvolveButton;
    M5.Display.fillRoundRect(b.x, b.y, b.width, b.height, 12, theme::kPanelActive);
    M5.Display.drawRoundRect(b.x, b.y, b.width, b.height, 12, theme::kGold);
    smallLabel(tr(Text::Evolve), b.x + b.width / 2, b.y + b.height / 2, theme::kGold, middle_center);
  }
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
} }
