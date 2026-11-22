#include "sprite.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp32s3/rom/miniz.h>
#include <esp_heap_caps.h>
#else
#include <zlib.h>
#endif

namespace pokedex {
namespace {
struct DecodedSprite {
  uint8_t* raw = nullptr;
  uint32_t capacity = 0;
  const uint8_t* source = nullptr;
  uint32_t compressedSize = 0, rawSize = 0;
  uint64_t lastUsed = 0;
};
constexpr unsigned kCacheSlots = 4;
DecodedSprite slots[kCacheSlots];
uint64_t useClock = 0;
int findSlot(const Sprite& sprite) {
  if (!sprite.data || !sprite.data_size || !sprite.raw_size) return -1;
  for (unsigned i = 0; i < kCacheSlots; ++i) {
    const auto& slot = slots[i];
    if (slot.source == sprite.data && slot.compressedSize == sprite.data_size &&
        slot.rawSize == sprite.raw_size) {
      return int(i);
    }
  }
  return -1;
}
const uint8_t* cache(const Sprite& sprite) {
  if (!sprite.data || !sprite.data_size || !sprite.raw_size) return nullptr;
  const int hit = findSlot(sprite);
  if (hit >= 0) {
    slots[hit].lastUsed = ++useClock;
    return slots[hit].raw;
  }
  unsigned target = 0;
  for (unsigned i = 0; i < kCacheSlots; ++i) {
    if (!slots[i].source) {
      target = i;
      break;
    }
    if (slots[i].lastUsed < slots[target].lastUsed) target = i;
  }
  auto& slot = slots[target];
  if (slot.capacity < sprite.raw_size) {
#if defined(ARDUINO_ARCH_ESP32)
    uint8_t* resized = static_cast<uint8_t*>(
        heap_caps_realloc(slot.raw, sprite.raw_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
    uint8_t* resized = static_cast<uint8_t*>(std::realloc(slot.raw, sprite.raw_size));
#endif
    if (!resized) return nullptr;
    slot.raw = resized;
    slot.capacity = sprite.raw_size;
  }
  slot.source = nullptr;
  slot.compressedSize = 0;
  slot.rawSize = 0;
  slot.lastUsed = 0;
#if defined(ARDUINO_ARCH_ESP32)
  // tinfl_decompressor is about 11 KB. The ROM helper keeps it on the caller
  // stack, which overflows the 8 KB Arduino loop task.
  static tinfl_decompressor* decomp = nullptr;
  if (!decomp) {
    decomp = static_cast<tinfl_decompressor*>(
        heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!decomp) return nullptr;
  }
  tinfl_init(decomp);
  size_t input = sprite.data_size;
  size_t output = sprite.raw_size;
  const tinfl_status status = tinfl_decompress(
      decomp, sprite.data, &input, slot.raw, slot.raw, &output,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  const bool ok = status == TINFL_STATUS_DONE && output == sprite.raw_size;
#else
  z_stream stream = {};
  if (inflateInit(&stream) != Z_OK) return nullptr;
  stream.next_in = const_cast<Bytef*>(sprite.data);
  stream.avail_in = sprite.data_size;
  stream.next_out = slot.raw;
  stream.avail_out = sprite.raw_size;
  const int result = inflate(&stream, Z_FINISH);
  const bool ok = result == Z_STREAM_END && stream.total_out == sprite.raw_size;
  inflateEnd(&stream);
#endif
  if (!ok) return nullptr;
  slot.source = sprite.data;
  slot.compressedSize = sprite.data_size;
  slot.rawSize = sprite.raw_size;
  slot.lastUsed = ++useClock;
  return slot.raw;
}
}

bool prefetchSprite(const Sprite& sprite) { return cache(sprite) != nullptr; }

bool isSpriteCached(const Sprite& sprite) { return findSlot(sprite) >= 0; }

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
      source_size > sprite.raw_size - frame.offset) return false;
  const uint8_t* raw = cache(sprite);
  if (!raw) return false;
  const uint8_t* src = raw + frame.offset;
  for (uint16_t y = 0; y < sprite.height; ++y, src += sprite.width) {
    uint16_t* row = pixels + size_t(y) * scale * width;
    uint16_t* dst = row;
    for (uint16_t x = 0; x < sprite.width; ++x, dst += scale) {
      const uint8_t index = src[x];
      if (index >= sprite.palette_size) return false;
      std::fill_n(dst, scale, sprite.palette[index]);
    }
    for (uint16_t dy = 1; dy < scale; ++dy) {
      std::memcpy(row + size_t(dy) * width, row, width * sizeof(uint16_t));
    }
  }
  return true;
}

}
