#include <cstdio>
#include <vector>
#include <zlib.h>

#include "../firmware/apps/20_pokedex/game.hpp"
#include "../firmware/apps/20_pokedex/sprites_generated.hpp"

int main() {
  std::vector<uint16_t> pixels(size_t(pokedex::kMaxSourceSize) * pokedex::kMaxSourceSize);
  std::vector<uint16_t> scaled(size_t(pokedex::kMaxDisplayWidth) * pokedex::kMaxDisplayHeight);
  std::vector<uint8_t> raw(size_t(pokedex::kMaxSourceSize) * pokedex::kMaxSourceSize * 2);
  for (size_t i = 0; i < pokedex::data::kSpriteCount; ++i) {
    const auto& s = pokedex::data::kSprites[i];
    uint32_t time = 0;
    pokedex::viewer::State state;
    const size_t pixelCount = size_t(s.width) * s.height;
    raw.resize(pixelCount * 2);
    for (uint16_t f = 0; f < s.frame_count; ++f) {
      pokedex::viewer::update(state, s, time);
      if (state.frame_index != f || !pokedex::decodeFrame(s,f,1,pixels.data(),pixels.size())) {
        std::fprintf(stderr, "decode failed id=%u frame=%u raw=%u\n", s.id, f, s.raw_size);
        return 1;
      }
      const uint16_t scale = pokedex::displayScale(s);
      if (!pokedex::decodeFrame(s, f, scale, scaled.data(), scaled.size())) return 3;
      const size_t width = size_t(s.width) * scale;
      for (size_t y = 0; y < size_t(s.height) * scale; ++y)
        for (size_t x = 0; x < width; ++x)
          if (scaled[y * width + x] != pixels[(y / scale) * s.width + x / scale]) return 4;
      for (size_t p = 0; p < pixelCount; ++p) {
        raw[2 * p] = uint8_t(pixels[p] & 0xFFu);
        raw[2 * p + 1] = uint8_t(pixels[p] >> 8);
      }
      const unsigned long frameCrc =
          static_cast<unsigned long>(crc32(0L, raw.data(), static_cast<uInt>(raw.size())) & 0xFFFFFFFFUL);
      std::printf("%u %u %u %u %u %08lx\n", s.id, f, s.width, s.height,
                  s.frames[f].duration_ms, frameCrc);
      time += s.frames[f].duration_ms;
    }
    if (time != s.duration_ms) return 2;
  }
}
