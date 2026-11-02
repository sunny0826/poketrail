#pragma once
#include "locale.hpp"
#include <cstdint>
namespace pokedex { namespace i18n {
enum class Text : uint16_t {
#define UI_MESSAGE(id, chinese, english) id,
#include "ui_messages.inc"
#undef UI_MESSAGE
  Count
};
const char* text(Language, Text);
} }
