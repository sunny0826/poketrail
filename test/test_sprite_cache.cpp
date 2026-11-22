#include "../firmware/apps/20_pokedex/sprite.hpp"
#include <zlib.h>
#include <cstdio>
#include <vector>

namespace {
unsigned failures = 0;
void check(bool ok, const char* message) {
  if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
}

int main() {
  using namespace pokedex;
  const uint16_t palette[] = {kTransparentKey, 0xFFFF, 0xF800};
  const Frame frames[] = {{0, 100}, {4, 100}};
  const uint8_t raw[][8] = {
      {0,1,2,0,2,1,0,2}, {2,0,1,2,1,0,2,1}, {1,2,0,1,0,2,1,0},
      {0,2,1,0,1,2,0,1}, {2,1,0,2,0,1,2,0}, {1,0,2,1,2,0,1,2},
  };
  constexpr unsigned kCount = 6;
  std::vector<uint8_t> compressed[kCount];
  Sprite sprites[kCount] = {};
  for (unsigned i = 0; i < kCount; ++i) {
    compressed[i].resize(compressBound(8));
    uLongf size = compressed[i].size();
    check(compress(compressed[i].data(), &size, raw[i], 8) == Z_OK, "fixture compresses");
    compressed[i].resize(size);
    sprites[i] = {"cache", "", uint16_t(i + 1), 2, 2, palette, 3,
                  compressed[i].data(), uint32_t(size), 8, frames, 2, 200, "", "", false};
  }
  uint16_t pixels[16];
  auto draw = [&](unsigned id, unsigned frame, unsigned scale) {
    check(decodeFrame(sprites[id], frame, scale, pixels, 16), "actor frame decodes");
    for (unsigned y = 0; y < 2 * scale; ++y)
      for (unsigned x = 0; x < 2 * scale; ++x)
        check(pixels[y * 2 * scale + x] == palette[raw[id][4 * frame + y / scale * 2 + x / scale]],
              "alternating actors retain their own pixels and transparency");
  };
  for (unsigned frame = 0; frame < 2; ++frame) {
    draw(0, frame % 2, 2);
    draw(0, frame % 2, 1);
    draw(1, frame % 2, 2);
  }
  draw(2, 0, 1); draw(1, 1, 1); draw(0, 0, 1);

  for (unsigned i = 0; i < 4; ++i)
    check(prefetchSprite(sprites[i]), "prefetchSprite succeeds for 4 slots");
  for (unsigned i = 0; i < 4; ++i)
    check(isSpriteCached(sprites[i]), "all 4 slots stay cached simultaneously");
  check(!isSpriteCached(sprites[4]) && !isSpriteCached(sprites[5]), "unloaded sprites are not cached");

  draw(0, 1, 2);
  check(prefetchSprite(sprites[4]), "5th sprite loads into cache");
  check(!isSpriteCached(sprites[1]), "hit refresh on slot 0 causes LRU slot 1 to be evicted");
  check(isSpriteCached(sprites[0]) && isSpriteCached(sprites[2]) &&
        isSpriteCached(sprites[3]) && isSpriteCached(sprites[4]),
        "remaining 3 older slots and new 5th sprite remain cached");

  check(prefetchSprite(sprites[2]), "prefetch hit refreshes slot 2");
  draw(5, 0, 1);
  check(!isSpriteCached(sprites[3]), "LRU slot 3 is evicted after slot 2 hit refresh");
  check(isSpriteCached(sprites[0]) && isSpriteCached(sprites[2]) &&
        isSpriteCached(sprites[4]) && isSpriteCached(sprites[5]),
        "4-slot LRU retains expected entries");

  const uint8_t corrupt[] = {120,156,0};
  Sprite broken = sprites[2]; broken.data = corrupt; broken.data_size = sizeof(corrupt);
  check(!prefetchSprite(broken), "corrupt data fails prefetch");
  check(!decodeFrame(broken, 0, 1, pixels, 16), "corrupt compressed data fails even with a reused species id");
  check(!isSpriteCached(broken), "corrupt sprite is never marked cached");
  check(isSpriteCached(sprites[2]) && isSpriteCached(sprites[4]) && isSpriteCached(sprites[5]),
        "corrupt failures reuse the empty slot without evicting remaining valid entries");

  Sprite empty = sprites[0]; empty.data = nullptr;
  check(!prefetchSprite(empty) && !isSpriteCached(empty), "null sprite data fails safely");
  draw(2, 1, 1); draw(0, 1, 1);
  std::printf("sprite cache checks: %u failures\n", failures);
  return failures ? 1 : 0;
}
