#include "pokedex_text.hpp"
#include "i18n.hpp"
#include "sprites_generated.hpp"
#include "pokedex_en_generated.hpp"
namespace pokedex { namespace i18n {
const char* speciesName(Language language, uint16_t id) {
  if (!id || id > data::kSpriteCount) return text(language, Text::NoSelection);
  return language == Language::English ? data::kEnglishSpecies[id - 1].name : data::kSprites[id - 1].name;
}
const char* speciesTypes(Language language, uint16_t id) {
  if (!id || id > data::kSpriteCount) return "";
  return language == Language::English ? data::kEnglishSpecies[id - 1].types : data::kSprites[id - 1].types;
}
} }
