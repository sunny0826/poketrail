#include "sprite.hpp"

#include <algorithm>
#include <cstdlib>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp32s3/rom/miniz.h>
#include <esp_heap_caps.h>
#else
#include <zlib.h>
#endif

namespace pokedex {
namespace {
uint8_t* raw_ = nullptr;
uint32_t raw_capacity_ = 0;
uint16_t cached_ = 0;
bool cache(const Sprite& sprite) {
  if (cached_ == sprite.id) return true;
  if (!sprite.data || !sprite.raw_size) return false;
  if (raw_capacity_ < sprite.raw_size) {
#if defined(ARDUINO_ARCH_ESP32)
    uint8_t* resized = static_cast<uint8_t*>(
        heap_caps_realloc(raw_, sprite.raw_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
    uint8_t* resized = static_cast<uint8_t*>(std::realloc(raw_, sprite.raw_size));
#endif
    if (!resized) return false;
    raw_ = resized;
    raw_capacity_ = sprite.raw_size;
    cached_ = 0;
  }
#if defined(ARDUINO_ARCH_ESP32)
  // tinfl_decompressor is about 11 KB. The ROM helper keeps it on the caller
  // stack, which overflows the 8 KB Arduino loop task.
  static tinfl_decompressor* decomp = nullptr;
  if (!decomp) {
    decomp = static_cast<tinfl_decompressor*>(
        heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!decomp) return false;
  }
  tinfl_init(decomp);
  size_t input = sprite.data_size;
  size_t output = sprite.raw_size;
  const tinfl_status status = tinfl_decompress(
      decomp, sprite.data, &input, raw_, raw_, &output,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  const bool ok = status == TINFL_STATUS_DONE && output == sprite.raw_size;
#else
  z_stream stream = {};
  if (inflateInit(&stream) != Z_OK) return false;
  stream.next_in = const_cast<Bytef*>(sprite.data);
  stream.avail_in = sprite.data_size;
  stream.next_out = raw_;
  stream.avail_out = sprite.raw_size;
  const int result = inflate(&stream, Z_FINISH);
  const bool ok = result == Z_STREAM_END && stream.total_out == sprite.raw_size;
  inflateEnd(&stream);
#endif
  if (!ok) return false;
  cached_ = sprite.id;
  return true;
}
}

uint16_t displayScale(const Sprite& sprite) {
  if (!sprite.width || !sprite.height ||
      std::max(sprite.width, sprite.height) > kMaxSourceSize) return 0;
  if (sprite.enlarged && sprite.width * 3 <= kMaxDisplayWidth &&
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
  const size_t source_size = size_t(sprite.width) * sprite.height;
  if (!sprite.palette_size || frame.offset > sprite.raw_size ||
      source_size > sprite.raw_size - frame.offset || !cache(sprite)) return false;
  for (size_t position = 0; position < source_size; ++position) {
    const uint8_t index = raw_[frame.offset + position];
    if (index >= sprite.palette_size) return false;
    const uint16_t color = sprite.palette[index];
    const size_t x = (position % sprite.width) * scale;
    const size_t y = (position / sprite.width) * scale;
    for (uint16_t dy = 0; dy < scale; ++dy)
      std::fill_n(pixels + (y + dy) * width + x, scale, color);
  }
  return true;
}

}  // namespace pokedex
