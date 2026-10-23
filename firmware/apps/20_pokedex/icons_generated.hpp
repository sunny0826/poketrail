#pragma once

// 由 tools/build_icons.py 自动生成，请勿手工编辑。
// 透明像素使用 sprite.hpp 的 pokedex::kTransparentKey。
#include <stdint.h>

namespace pokedex {
namespace data {

struct Icon {
  uint16_t width;
  uint16_t height;
  const uint16_t* pixels;
};

extern const Icon kPokemonIcons[151];
extern const Icon kPokeBallIcon;

}  // namespace data
}  // namespace pokedex
