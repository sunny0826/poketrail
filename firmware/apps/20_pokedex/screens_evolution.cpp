#include "screens.hpp"
#include "board.hpp"
#include <cmath>

namespace pokedex { namespace ui {
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
  title("进化预览", speciesName(view.from), theme::kGold);
  thumbnail(view.from, 233, 140, 2);
  for (unsigned i = 0; i < view.count; ++i)
    item(s, i, speciesName(view.options[i].target), menu_layout::kTop, view.options[i].target);
  item(s, view.count, "返回");
}
bool Screens::evolution(const ScreenState& s, uint32_t now) {
  const auto& anim = s.evolutionAnim;
  const auto frame = evolutionFrameAt(anim, now);
  const bool revealed = frame.stage == EvolutionStage::Reveal || frame.stage == EvolutionStage::Done;
  title(s.page == Page::EvolutionPreview ? "进化预览" : revealed ? "进化完成" : "正在进化",
        nullptr, theme::kGold);
  M5.Display.drawCircle(233, 233, 155, theme::kPanelActive);
  M5.Display.drawCircle(233, 233, 160, theme::kPanel);

  if (!battlePlayback_.species) advance(battlePlayback_, anim.from, anim.startMs);
  if (!enemyBattlePlayback_.species) advance(enemyBattlePlayback_, anim.to, anim.startMs);
  advance(battlePlayback_, anim.from, now);
  advance(enemyBattlePlayback_, anim.to, now);

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
  line(revealed ? "按键 / 轻触继续" : "按键 / 轻触跳过", 411, theme::kMuted);
  return rendered;
}
} }
