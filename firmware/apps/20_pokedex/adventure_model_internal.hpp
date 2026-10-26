#pragma once
#include "adventure_model.hpp"

// Helpers shared between the adventure model translation units. They are not
// part of the public save API.
namespace pokedex { namespace adventure {
// Calendar date equivalence used by clock readings and stamped saves.
bool validDate(int64_t utcSeconds, int16_t offsetMinutes, uint32_t dayKey);
// Schema 1/2 saves never encode the activity block; schema 3 validates it.
bool noActivityHistory(const Save&);
bool validActivity(const Save&);
} }
