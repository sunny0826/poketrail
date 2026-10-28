#pragma once
#include "adventure_model.hpp"
namespace pokedex { namespace adventure {
bool migrateLegacy(Save&);
void upgradeToSchema4(Save&);
void upgradeToSchema5(Save&);
} }
