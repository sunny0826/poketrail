#include "encounter_catalog.hpp"
#include "adventure_catalog_generated.hpp"
#include "progression_catalog_generated.hpp"
#include "route_masks_v4_generated.hpp"
namespace pokedex { namespace adventure {
static_assert(sizeof(kCatalog) / sizeof(kCatalog[0]) ==
              sizeof(kOrdinaryEncounter) / sizeof(kOrdinaryEncounter[0]),
              "catalog and ordinary pool must list the same species");
constexpr uint16_t kListedSpecies = sizeof(kCatalog) / sizeof(kCatalog[0]);
constexpr uint16_t kLegacyRouteEntries = sizeof(kRouteMaskV4) / sizeof(kRouteMaskV4[0]);
const CatalogEntry* catalogEntry(uint16_t species) {
  return species >= 1 && species <= kListedSpecies ? &kCatalog[species - 1] : nullptr;
}
uint8_t savedRouteMask(uint16_t species, uint16_t schema) {
  if (species < 1 || species > speciesCapacity(schema).count) return 0;
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
static bool eligible(const Save& s, Route route, uint16_t id) {
  return ordinaryEncounter(id) && !s.captureCounts[id - 1] &&
         (kCatalog[id - 1].routes & (1u << uint8_t(route)));
}
unsigned remainingEncounters(const Save& s, Route route) {
  if (uint8_t(route) > 2) return 0;
  unsigned count = 0;
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) count += eligible(s, route, id);
  return count;
}
uint16_t chooseEncounter(const Save& s, Route route, uint32_t seed) {
  if (uint8_t(route) > 2) return 0;
  uint32_t total = 0;
  for (uint16_t i = 1; i <= kSpeciesCount; ++i) {
    if (!eligible(s, route, i)) continue;
    const CatalogEntry& e = kCatalog[i - 1];
    total += uint32_t(e.weight) * (owns(s, i) ? 1 : 3);
  }
  if (!total) return 0;
  uint32_t draw = mixRandom(seed) % total;
  for (uint16_t i = 1; i <= kSpeciesCount; ++i) {
    if (!eligible(s, route, i)) continue;
    const CatalogEntry& e = kCatalog[i - 1];
    const uint32_t weight = uint32_t(e.weight) * (owns(s, i) ? 1 : 3);
    if (draw < weight) return i;
    draw -= weight;
  }
  return 0;
}
} }
