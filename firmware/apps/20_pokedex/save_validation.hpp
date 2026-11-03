#pragma once
#include "adventure_model.hpp"

namespace pokedex { namespace adventure { namespace save_validation {
inline bool cleanBitmap(const std::array<uint8_t, kSpeciesBytes>& bits, uint16_t count) {
  for (size_t byte = 0; byte < kSpeciesBytes; ++byte) {
    const uint16_t first = uint16_t(byte * 8 + 1);
    if (first > count) { if (bits[byte]) return false; continue; }
    const uint16_t used = uint16_t(count - first + 1);
    const uint8_t mask = used >= 8 ? 0xFF : uint8_t((1u << used) - 1);
    if (bits[byte] & uint8_t(~mask)) return false;
  }
  return true;
}
} } }
