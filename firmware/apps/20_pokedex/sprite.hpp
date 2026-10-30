#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pokedex {

constexpr uint16_t kTransparentKey = 0xF81F;
constexpr uint16_t kMaxSourceSize = 160;  // Measured through #649: 153 wide, 117 tall.
constexpr uint16_t kMaxDisplayWidth = 336;
constexpr uint16_t kMaxDisplayHeight = 270;

// data 是该宝可梦全部去重帧的 zlib 数据。Frame::offset 指向解压后的调色板索引。
struct Frame {
  uint32_t offset;
  uint16_t duration_ms;
};

struct Sprite {
  const char* name;
  const char* types;
  uint16_t id;
  uint16_t width;
  uint16_t height;
  const uint16_t* palette;
  uint16_t palette_size;
  const uint8_t* data;
  uint32_t data_size;
  uint32_t raw_size;
  const Frame* frames;
  uint16_t frame_count;
  uint32_t duration_ms;
  const char* height_text;
  const char* weight_text;
  // 优先 3 倍类别：中间／最终形态与目录内无进化链物种；
  // 实际缩放仍受 336x270 画布与页面高度限制。
  bool enlarged;
};

uint16_t displayScale(const Sprite& sprite);
bool decodeFrame(const Sprite& sprite, uint16_t frame, uint16_t scale,
                 uint16_t* pixels, size_t capacity);

}  // namespace pokedex
