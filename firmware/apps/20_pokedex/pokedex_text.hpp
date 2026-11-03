#pragma once
#include "locale.hpp"
#include <cstdint>
namespace pokedex { namespace i18n {
const char* speciesName(Language, uint16_t id);
const char* speciesTypes(Language, uint16_t id);
} }
