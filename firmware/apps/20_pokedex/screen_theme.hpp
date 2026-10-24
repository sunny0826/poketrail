#pragma once
#include <stdint.h>
#include "adventure_model.hpp"

namespace pokedex { namespace ui { namespace theme {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
constexpr uint16_t kBackground = rgb(0x08, 0x13, 0x11);
constexpr uint16_t kPanel = rgb(0x11, 0x23, 0x1E);
constexpr uint16_t kPanelActive = rgb(0x19, 0x35, 0x2C);
constexpr uint16_t kText = rgb(0xF4, 0xF0, 0xE4);
constexpr uint16_t kMuted = rgb(0x8D, 0xA5, 0x9A);
constexpr uint16_t kMint = rgb(0x7C, 0xE3, 0xB1);
constexpr uint16_t kGold = rgb(0xF4, 0xC9, 0x5D);
constexpr uint16_t kRed = rgb(0xFF, 0x82, 0x78);
constexpr uint16_t kBlue = rgb(0x7A, 0xC9, 0xE8);
constexpr uint16_t kPurple = rgb(0xB8, 0xA4, 0xE3);
constexpr uint16_t kShadow = rgb(0x13, 0x25, 0x1F);
constexpr uint16_t kTrack = rgb(0x5B, 0x71, 0x68);
constexpr uint16_t kDisabled = rgb(0x30, 0x44, 0x3C);
constexpr uint16_t kOutline = rgb(0x29, 0x43, 0x3B);
constexpr uint16_t kAlertBackground = rgb(0x25, 0x30, 0x22);
constexpr uint16_t kViewerBackground = rgb(0x09, 0x15, 0x12);
constexpr uint16_t kViewerRing = rgb(0x20, 0x3B, 0x32);
constexpr uint16_t kViewerRingInner = rgb(0x16, 0x2C, 0x25);

inline uint16_t routeColor(adventure::Route route) {
  if (route == adventure::Route::Coast) return kBlue;
  if (route == adventure::Route::Cave) return kPurple;
  return kMint;
}

enum class Icon : uint8_t {
  Dot, Route, Partner, Book, Records, Clock, Practice, Sleep, Home, Back, Next,
};

} } }
