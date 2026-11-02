#pragma once
#include <cstdint>

namespace pokedex { namespace i18n {
enum class Language : uint8_t { Chinese = 0, English = 1 };
constexpr bool validLanguage(Language language) {
  return language == Language::Chinese || language == Language::English;
}
} }
