#include "screens.hpp"
#include "board.hpp"
#include <cmath>

namespace pokedex { namespace ui {
using i18n::Text;
namespace {
void sparkle(int x, int y, int size, uint16_t color) {
  M5.Display.fillRect(x - size, y - 1, size * 2 + 2, 2, color);
  M5.Display.fillRect(x - 1, y - size, 2, size * 2 + 2, color);
}
void particles(uint32_t elapsed, bool outward) {
  for (unsigned i = 0; i < 12; ++i) {
    const unsigned phase = (elapsed + i * 73) % 800;
    const int radius = outward ? 75 + phase / 10 : 165 - phase / 8;
    const float angle = i * 6.2831853f / 12 + elapsed * 0.0003f;
    const int x = 233 + int(std::cos(angle) * radius) / 2 * 2;
    const int y = 233 + int(std::sin(angle) * radius) / 2 * 2;
    sparkle(x, y, phase < 400 ? 4 : 2, i % 3 ? theme::kGold : theme::kText);
  }
}
}
void Screens::evolutionPreviewTargets(const ScreenState& s, const adventure::Engine& e) {
  const auto view = e.evolutionView(s.evolutionAnim.from);
  title(tr(Text::EvolutionPreview), speciesName(view.from), theme::kGold);
  thumbnail(view.from, 233, 140, 2);
  for (unsigned i = 0; i < view.count; ++i)
    item(s, i, speciesName(view.options[i].target), menu_layout::kTop, view.options[i].target);
  item(s, view.count, tr(Text::Back));
}
bool Screens::evolution(const ScreenState& s, uint32_t now) {
  const auto& anim = s.evolutionAnim;
  const auto frame = evolutionFrameAt(anim, now);
  const bool revealed = frame.stage == EvolutionStage::Reveal || frame.stage == EvolutionStage::Done;
  title(s.page == Page::EvolutionPreview ? tr(Text::EvolutionPreview) : revealed ? tr(Text::EvolutionComplete) : tr(Text::Evolving),
        nullptr, theme::kGold);
  M5.Display.drawCircle(233, 233, 155, theme::kPanelActive);
  M5.Display.drawCircle(233, 233, 160, theme::kPanel);

  auto syncEvo = [&](Playback& pb, uint16_t sp) {
    if (pb.species != sp || pb.state.animation_started_ms != anim.startMs) {
      pb.species = 0;
      advance(pb, sp, anim.startMs);
    }
    advance(pb, sp, now);
  };
  syncEvo(battlePlayback_, anim.from);
  syncEvo(enemyBattlePlayback_, anim.to);

  if (frame.stage == EvolutionStage::Burst) {
    const unsigned half = (kEvolutionRevealMs - kEvolutionBurstMs) / 2;
    const unsigned wave = frame.elapsed < half ? frame.elapsed : half * 2 - frame.elapsed;
    M5.Display.fillCircle(233, 233, 50 + int(wave * 75 / half), theme::kText);
  } else {
    const uint32_t elapsed = now - anim.startMs;
    particles(elapsed, revealed);
    if (!revealed) {
      const int radius = 65 + int((elapsed % 800) * 75 / 800);
      M5.Display.drawCircle(233, 233, radius, theme::kGold);
      M5.Display.drawCircle(233, 233, radius + 3, theme::kGold);
    }
  }
  const uint16_t id = frame.target ? anim.to : anim.from;
  const bool glowing = frame.stage == EvolutionStage::Morph || frame.stage == EvolutionStage::Burst ||
                       (frame.stage == EvolutionStage::Gather && frame.elapsed >= 520);
  const bool rendered = sprite(id, now, frame.target ? enemyBattlePlayback_ : battlePlayback_,
                               233, 233, glowing ? theme::kText : 0, 3, 250);
  fitLine(speciesName(revealed ? anim.to : anim.from), 379, 300, revealed ? theme::kMint : theme::kText);
  line(revealed ? tr(Text::PressContinue) : tr(Text::PressSkip), 411, theme::kMuted);
  return rendered;
}
void Screens::drawEvolutionTargetRow(const ScreenState& s, const adventure::Engine& e,
                                     unsigned index, int top) {
  const auto view = e.evolutionView(s.evolutionAnim.from);
  if (index < view.count)
    item(s, index, speciesName(view.options[index].target), top, view.options[index].target);
  else
    item(s, view.count, tr(Text::Back), top);
}
void Screens::drawEvolutionTargets(const ScreenState& s, const adventure::Engine& e, bool full) {
  const auto view = e.evolutionView(s.evolutionAnim.from);
  if (full) {
    evolutionPreviewTargets(s, e);
  } else if (s.evolutionAnim.from != snap_.evoFrom || e.view().rosterRevision != snap_.rosterRevision) {
    WriteScope sc;
    clearRegion(0, 36, 468, 405, theme::kBackground);
    evolutionPreviewTargets(s, e);
    if (1 + view.count > menu_layout::kPageSize) pager(s, 1 + view.count);
    glyph(theme::Icon::Back, 112, 52, theme::kMuted);
  } else {
    updateListRows(s, e, menu_layout::kTop, true, 1 + view.count, &Screens::drawEvolutionTargetRow);
  }
  snap_.evoFrom = s.evolutionAnim.from;
  snap_.rosterRevision = e.view().rosterRevision;
}
bool Screens::drawEvolution(const ScreenState& s, uint32_t now, bool full) {
  const auto& anim = s.evolutionAnim;
  const auto frame = evolutionFrameAt(anim, now);
  const bool revealed = frame.stage == EvolutionStage::Reveal || frame.stage == EvolutionStage::Done;
  bool rendered = true;
  if (full) {
    rendered = evolution(s, now);
  } else if (anim.from != snap_.evoFrom || anim.to != snap_.evoTo) {
    WriteScope sc;
    clearRegion(0, 36, 468, 405, theme::kBackground);
    rendered = evolution(s, now);
  } else {
    if (s.page == Page::Evolution && revealed != snap_.evoRevealed) {
      WriteScope sc;
      clearRegion(100, 36, 268, 28, theme::kBackground);
      title(revealed ? tr(Text::EvolutionComplete) : tr(Text::Evolving), nullptr, theme::kGold);
    }
    WriteScope sc;
    clearRegion(64, 64, 338, 340, theme::kBackground);
    if (revealed != snap_.evoRevealed) clearRegion(60, 404, 348, 16, theme::kBackground);
    M5.Display.drawCircle(233, 233, 155, theme::kPanelActive);
    M5.Display.drawCircle(233, 233, 160, theme::kPanel);
    auto syncEvo = [&](Playback& pb, uint16_t sp) {
      if (pb.species != sp || pb.state.animation_started_ms != anim.startMs) {
        pb.species = 0;
        advance(pb, sp, anim.startMs);
      }
      advance(pb, sp, now);
    };
    syncEvo(battlePlayback_, anim.from);
    syncEvo(enemyBattlePlayback_, anim.to);
    if (frame.stage == EvolutionStage::Burst) {
      const unsigned half = (kEvolutionRevealMs - kEvolutionBurstMs) / 2;
      const unsigned wave = frame.elapsed < half ? frame.elapsed : half * 2 - frame.elapsed;
      M5.Display.fillCircle(233, 233, 50 + int(wave * 75 / half), theme::kText);
    } else {
      const uint32_t elapsed = now - anim.startMs;
      particles(elapsed, revealed);
      if (!revealed) {
        const int radius = 65 + int((elapsed % 800) * 75 / 800);
        M5.Display.drawCircle(233, 233, radius, theme::kGold);
        M5.Display.drawCircle(233, 233, radius + 3, theme::kGold);
      }
    }
    const uint16_t id = frame.target ? anim.to : anim.from;
    const bool glowing = frame.stage == EvolutionStage::Morph || frame.stage == EvolutionStage::Burst ||
                         (frame.stage == EvolutionStage::Gather && frame.elapsed >= 520);
    rendered = sprite(id, now, frame.target ? enemyBattlePlayback_ : battlePlayback_,
                      233, 233, glowing ? theme::kText : 0, 3, 250);
    fitLine(speciesName(revealed ? anim.to : anim.from), 379, 300, revealed ? theme::kMint : theme::kText);
    line(revealed ? tr(Text::PressContinue) : tr(Text::PressSkip), 411, theme::kMuted);
  }
  snap_.evoFrom = anim.from;
  snap_.evoTo = anim.to;
  snap_.evoRevealed = revealed;
  return rendered;
}
} }
