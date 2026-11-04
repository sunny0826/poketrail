#pragma once
#include "i18n.hpp"
#include "pokedex_text.hpp"
#include <cstring>
#include <string>

namespace language_catalogue {
using pokedex::i18n::Language;
using pokedex::i18n::Text;
struct Message { Text id; const char* chinese; const char* english; };
const Message messages[] = {
#define UI_MESSAGE(id, chinese, english) {Text::id, chinese, english},
#include "ui_messages.inc"
#undef UI_MESSAGE
};
inline std::vector<std::string> arguments(const char* text) {
  std::vector<std::string> result;
  for (const char* p = text; *p; ++p) {
    if (*p != '%') continue;
    ++p;
    if (*p == '%') continue;
    while (*p && std::strchr("-+ #0", *p)) ++p;
    while (*p && (std::strchr("0123456789.", *p) || *p == '*')) {
      if (*p == '*') result.push_back("int");
      ++p;
    }
    std::string type;
    while (*p && std::strchr("hljztL", *p)) type += *p++;
    progression_ui::check(*p && std::strchr("diuoxXfFeEgGaAcsp", *p), "message has a supported printf conversion");
    type += *p; result.push_back(type);
  }
  return result;
}
inline bool glyphs(const char* value, const lgfx::IFont* font) {
  lgfx::FontMetrics metrics; font->getDefaultMetric(&metrics);
  bool supported = true;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(value);
  while (*p) {
    uint32_t code = *p++;
    unsigned tail = 0;
    if (code >= 0xF0) { tail = 3; code &= 7; }
    else if (code >= 0xE0) { tail = 2; code &= 15; }
    else if (code >= 0xC0) { tail = 1; code &= 31; }
    for (unsigned i = 0; i < tail; ++i) {
      progression_ui::check((*p & 0xC0) == 0x80, "catalogue text contains valid UTF-8 continuation");
      code = (code << 6) | (*p++ & 63);
    }
    if (code == '\n' || code == '\r') continue;
    if (code > 0xFFFF || !font->updateFontMetric(&metrics, uint16_t(code))) {
      std::fprintf(stderr, "Missing U+%04X in %s\n", unsigned(code), value);
      supported = false;
    }
  }
  return supported;
}
inline void verify() {
  using progression_ui::check;
  bool supported = true;
  check(sizeof(messages) / sizeof(messages[0]) == unsigned(Text::Count), "every message has both language entries");
  for (const auto& message : messages) {
    check(*message.chinese && *message.english, "paired messages are nonempty");
    check(arguments(message.chinese) == arguments(message.english), "Chinese and English printf argument order and types match");
    supported = glyphs(message.english, &fonts::efontCN_16) && supported;
  }
  for (uint16_t id = 1; id <= 649; ++id) {
    const char* name = pokedex::i18n::speciesName(Language::English, id);
    const char* types = pokedex::i18n::speciesTypes(Language::English, id);
    check(*name && *types, "every English species has a name and types");
    supported = glyphs(name, &fonts::efontCN_24) && supported;
    supported = glyphs(types, &fonts::efontCN_16) && supported;
  }
  check(supported, "all English glyphs are present in the production fonts");
}
}
