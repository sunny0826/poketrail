#include "encounter_catalog.hpp"
#include "adventure_catalog_generated.hpp"
#include "progression_catalog_generated.hpp"
#include "route_masks_v4_generated.hpp"
namespace pokedex { namespace adventure {
static_assert(sizeof(kCatalog) / sizeof(kCatalog[0]) ==
              sizeof(kOrdinaryEncounter) / sizeof(kOrdinaryEncounter[0]),
              "catalog and ordinary pool must list the same species");
static_assert(sizeof(kRegionRanges) / sizeof(kRegionRanges[0]) == kRegionCount,
              "catalog must define every region");
constexpr uint16_t kListedSpecies = sizeof(kCatalog) / sizeof(kCatalog[0]);
constexpr uint16_t kLegacyRouteEntries = sizeof(kRouteMaskV4) / sizeof(kRouteMaskV4[0]);
const CatalogEntry* catalogEntry(uint16_t species) {
  return species >= 1 && species <= kListedSpecies ? &kCatalog[species - 1] : nullptr;
}
Route regionOf(uint16_t species) {
  for (uint8_t i = 0; i < kRegionCount; ++i)
    if (species >= kRegionRanges[i].first && species <= kRegionRanges[i].last) return regionAt(i);
  return Route::Forest;
}
uint8_t savedRouteMask(uint16_t species, uint16_t schema) {
  if (species < 1 || species > speciesCapacity(schema).count) return 0;
  if (schema >= 7) return uint8_t(1u << uint8_t(regionOf(species)));
  if (schema >= 5) return species <= kListedSpecies ? kCatalog[species - 1].routes : 0;
  return species <= kLegacyRouteEntries ? kRouteMaskV4[species - 1] : 0;
}
bool ordinaryEncounter(uint16_t species) {
  return species >= 1 && species <= kListedSpecies && kOrdinaryEncounter[species - 1];
}
const EvolutionRule* evolutionRules(size_t& count) {
  count = sizeof(kEvolutionRules) / sizeof(kEvolutionRules[0]);
  return kEvolutionRules;
}
const EvolutionRule* evolutionRule(uint16_t from, uint16_t to) {
  for (const auto& rule : kEvolutionRules)
    if (rule.from == from && rule.to == to) return &rule;
  return nullptr;
}
using SpeciesMask = std::array<uint8_t, kSpeciesBytes>;
static bool marked(const SpeciesMask& mask, uint16_t id) {
  return mask[(id - 1) / 8] & (1u << ((id - 1) % 8));
}
static SpeciesMask retiredSpecies(const Save& s) {
  SpeciesMask retired = s.registered;
  // Walk backwards to a fixed point: edges are not ordered by ancestry or dex ID.
  // Only ancestors inherit retirement; unregistered siblings remain available.
  bool changed;
  do {
    changed = false;
    for (const auto& rule : kEvolutionRules) {
      if (!marked(retired, rule.to) || marked(retired, rule.from)) continue;
      retired[(rule.from - 1) / 8] |= uint8_t(1u << ((rule.from - 1) % 8));
      changed = true;
    }
  } while (changed);
  return retired;
}
static bool eligible(const Save& s, const SpeciesMask& retired, Route region, uint16_t id) {
  if (!ordinaryEncounter(id) || s.captureCounts[id - 1] || marked(retired, id) || regionOf(id) != region)
    return false;
  if (s.phase != Phase::Walking && s.encounter.species == id) return false;
  for (size_t i = 0; i < s.encounterQueueSize; ++i)
    if (s.encounterQueue[i].species == id) return false;
  return true;
}
unsigned remainingEncounters(const Save& s, Route region) {
  if (!isRegion(region)) return 0;
  const SpeciesMask retired = retiredSpecies(s);
  const RegionRange& span = kRegionRanges[regionIndex(region)];
  unsigned count = 0;
  for (uint16_t id = span.first; id <= span.last; ++id) count += eligible(s, retired, region, id);
  return count;
}
uint16_t chooseEncounter(const Save& s, Route region, uint32_t seed) {
  if (!isRegion(region)) return 0;
  const SpeciesMask retired = retiredSpecies(s);
  const RegionRange& span = kRegionRanges[regionIndex(region)];
  uint32_t total = 0;
  for (uint16_t i = span.first; i <= span.last; ++i) {
    if (!eligible(s, retired, region, i)) continue;
    total += uint32_t(kCatalog[i - 1].weight) * (owns(s, i) ? 1 : 3);
  }
  if (!total) return 0;
  uint32_t draw = mixRandom(seed) % total;
  for (uint16_t i = span.first; i <= span.last; ++i) {
    if (!eligible(s, retired, region, i)) continue;
    const uint32_t weight = uint32_t(kCatalog[i - 1].weight) * (owns(s, i) ? 1 : 3);
    if (draw < weight) return i;
    draw -= weight;
  }
  return 0;
}
} }
