#include "i18n.hpp"
#include <cstddef>
namespace pokedex { namespace i18n {
namespace {
struct Message { const char* chinese; const char* english; };
constexpr Message messages[] = {
#define UI_MESSAGE(id, chinese, english) {chinese, english},
#include "ui_messages.inc"
#undef UI_MESSAGE
};
static_assert(sizeof(messages) / sizeof(messages[0]) == size_t(Text::Count), "complete UI messages");
}
const char* text(Language language, Text id) {
  const auto index = size_t(id);
  if (index >= size_t(Text::Count)) return "";
  return language == Language::English ? messages[index].english : messages[index].chinese;
}
} }
