#include "../firmware/apps/20_pokedex/sprite.hpp"
#include <zlib.h>
#include <cstdio>
#include <vector>

namespace {
unsigned inflations = 0, failures = 0;
void check(bool ok, const char* message) {
  if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
}
extern "C" int poketrail_test_inflate(z_streamp stream, int flush) {
  ++inflations;
  return inflate(stream, flush);
}

int main() {
  using namespace pokedex;
  const uint16_t palette[] = {kTransparentKey, 0xFFFF, 0xF800};
  const Frame frames[] = {{0, 100}, {4, 100}};
  const uint8_t raw[][8] = {{0,1,2,0,2,1,0,2}, {2,0,1,2,1,0,2,1}, {1,2,0,1,0,2,1,0}};
  std::vector<uint8_t> compressed[3];
  Sprite sprites[3] = {};
  for (unsigned i = 0; i < 3; ++i) {
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
  for (unsigned frame = 0; frame < 100; ++frame) {
    draw(0, frame % 2, 2);
    draw(0, frame % 2, 1);
    draw(1, frame % 2, 2);
  }
  std::printf("100 dual-actor frames: %u inflations\n", inflations);
  check(inflations == 2, "two actors decompress once each, including frame and scale changes");
  draw(2, 0, 1); draw(1, 1, 1); draw(0, 0, 1);
  check(inflations == 4, "third species replaces the least recently used actor");
  // A malformed replacement must not leave its partially written data under an old cache key.
  const uint8_t corrupt[] = {120,156,0};
  Sprite broken = sprites[1]; broken.data = corrupt; broken.data_size = sizeof(corrupt);
  check(!decodeFrame(broken, 0, 1, pixels, 16), "corrupt compressed data fails even with a reused species id");
  draw(1, 0, 1); draw(0, 1, 1);
  std::printf("sprite cache checks: %u failures\n", failures);
  return failures ? 1 : 0;
}
