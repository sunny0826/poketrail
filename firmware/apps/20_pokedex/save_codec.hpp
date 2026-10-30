#pragma once
#include "adventure_model.hpp"
namespace pokedex { namespace adventure { namespace codec {
constexpr size_t kHeaderSize = 22;
constexpr size_t maxEncodedSize = 49152;
enum class DecodeStatus : uint8_t { Ok, Corrupt, UnsupportedVersion, IoError };
bool encode(const Save&, uint8_t* out, size_t capacity, size_t& written);
DecodeStatus decode(const uint8_t*, size_t length, Save& out);
uint32_t crc32(const uint8_t*, size_t);
} } }
