#pragma once
#include "adventure_model.hpp"
namespace pokedex { namespace adventure {
struct CatalogEntry { uint8_t routes; bool rare; uint16_t weight; };
struct EvolutionRule { uint16_t from, to; uint32_t steps; };
const CatalogEntry* catalogEntry(uint16_t species);
uint8_t savedRouteMask(uint16_t species, uint16_t schema);
bool ordinaryEncounter(uint16_t species);
const EvolutionRule* evolutionRule(uint16_t from, uint16_t to);
const EvolutionRule* evolutionRules(size_t& count);
unsigned remainingEncounters(const Save&, Route);
uint16_t chooseEncounter(const Save&, Route, uint32_t seed);
} }
