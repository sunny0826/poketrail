#pragma once
#include <cstdint>

namespace pokedex { namespace ui {
constexpr uint32_t kEvolutionMorphMs = 800;
constexpr uint32_t kEvolutionBurstMs = 2800;
constexpr uint32_t kEvolutionRevealMs = 3200;
constexpr uint32_t kEvolutionTotalMs = 4800;

enum class EvolutionStage : uint8_t { Gather, Morph, Burst, Reveal, Done };
struct EvolutionAnim {
  uint16_t from = 0, to = 0;
  uint32_t startMs = 0;
};
struct EvolutionFrame {
  EvolutionStage stage;
  uint32_t elapsed;
  bool target;
};
inline EvolutionFrame evolutionFrameAt(const EvolutionAnim& anim, uint32_t now) {
  const uint32_t elapsed = now - anim.startMs;
  if (elapsed < kEvolutionMorphMs) return {EvolutionStage::Gather, elapsed, false};
  if (elapsed < kEvolutionBurstMs) {
    const uint32_t t = elapsed - kEvolutionMorphMs;
    const uint32_t interval = t < 800 ? 200 : t < 1400 ? 120 : 80;
    return {EvolutionStage::Morph, t, (t / interval) % 2 != 0};
  }
  if (elapsed < kEvolutionRevealMs)
    return {EvolutionStage::Burst, elapsed - kEvolutionBurstMs, true};
  if (elapsed < kEvolutionTotalMs)
    return {EvolutionStage::Reveal, elapsed - kEvolutionRevealMs, true};
  return {EvolutionStage::Done, 0, true};
}
inline void skipEvolution(EvolutionAnim& anim, uint32_t now) {
  const auto stage = evolutionFrameAt(anim, now).stage;
  anim.startMs = now - (stage == EvolutionStage::Reveal || stage == EvolutionStage::Done
                         ? kEvolutionTotalMs : kEvolutionRevealMs);
}
} }
