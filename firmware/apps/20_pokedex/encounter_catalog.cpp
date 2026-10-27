#include "encounter_catalog.hpp"
#include "adventure_catalog_generated.hpp"
#include "progression_catalog_generated.hpp"
namespace pokedex { namespace adventure {
const CatalogEntry* catalogEntry(uint16_t species) {
  return species >= 1 && species <= kSpeciesCount ? &kCatalog[species - 1] : nullptr;
}
bool ordinaryEncounter(uint16_t species) {
  return species >= 1 && species <= kSpeciesCount && kOrdinaryEncounter[species - 1];
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
