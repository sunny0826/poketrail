#include "sprite.hpp"

#include <algorithm>

namespace pokedex {

uint16_t displayScale(const Sprite& sprite) {
  if (!sprite.width || !sprite.height ||
      std::max(sprite.width, sprite.height) > kMaxSourceSize) return 0;
  if (sprite.has_evolved && sprite.width * 3 <= kMaxDisplayWidth &&
      sprite.height * 3 <= kMaxDisplayHeight) return 3;
  return 2;
}

bool decodeFrame(const Sprite& sprite, uint16_t frame_index, uint16_t scale,
                 uint16_t* pixels, size_t capacity) {
  if (!pixels || !sprite.palette || !sprite.data || !sprite.frames ||
      frame_index >= sprite.frame_count || !scale || !sprite.width ||
      !sprite.height || sprite.width > kMaxSourceSize ||
      sprite.height > kMaxSourceSize) return false;
  const size_t width = size_t(sprite.width) * scale;
  const size_t height = size_t(sprite.height) * scale;
  if (width > kMaxDisplayWidth || height > kMaxDisplayHeight ||
      width * height > capacity) return false;
  const Frame& frame = sprite.frames[frame_index];
  if (frame.offset > sprite.data_size || frame.size > sprite.data_size - frame.offset)
    return false;

  const size_t source_size = size_t(sprite.width) * sprite.height;
  size_t position = 0;
  for (size_t i = 0; i < frame.size; ++i) {
    const uint8_t run = sprite.data[frame.offset + i];
    const size_t count = (run >> 4) + 1;
    if (count > source_size - position) return false;
    const uint16_t color = sprite.palette[run & 15];
    for (size_t n = 0; n < count; ++n, ++position) {
      const size_t x = (position % sprite.width) * scale;
      const size_t y = (position / sprite.width) * scale;
      for (uint16_t dy = 0; dy < scale; ++dy) {
        std::fill_n(pixels + (y + dy) * width + x, scale, color);
      }
    }
  }
  return position == source_size;
}

}  // namespace pokedex
