#pragma once
namespace pokedex { namespace ui { namespace menu_layout {
constexpr unsigned kPageSize = 3;
constexpr int kTop = 238;
constexpr int kResultTop = 355;
constexpr int kCaptureResultTop = 372;
constexpr int kPitch = 68;
constexpr int kWidth = 320;
constexpr int kHeight = 60;
constexpr int kCenter = 233;
// Row corners stay 8px inside the 228px visible radius; lower rows narrow with the circle.
constexpr int kRowRadius = 220;
constexpr int kIconInset = 40;
struct Rect {
  int x, y, width, height;
  bool contains(int px, int py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};
inline int center(unsigned row, int top = kTop) { return top + int(row) * kPitch; }
inline Rect rowBounds(unsigned row, int top = kTop) {
  const int y = center(row, top) - kHeight / 2;
  const int above = kCenter - y, below = y + kHeight - 1 - kCenter;
  const int dy = above > below ? above : below;
  int half = kWidth / 2;
  while (half > 0 && half * half + dy * dy > kRowRadius * kRowRadius) --half;
  return {kCenter - half, y, half * 2, kHeight};
}
inline int rowAt(int x, int y, int top = kTop) {
  for (unsigned row = 0; row < kPageSize; ++row)
    if (rowBounds(row, top).contains(x, y)) return int(row);
  return -1;
}
inline unsigned first(unsigned selected) { return selected / kPageSize * kPageSize; }
inline unsigned pageCount(unsigned count) { return (count + kPageSize - 1) / kPageSize; }
inline unsigned nextPage(unsigned selected, unsigned count) {
  return ((selected / kPageSize + 1) % pageCount(count)) * kPageSize;
}
inline unsigned previousPage(unsigned selected, unsigned count) {
  return ((selected / kPageSize + pageCount(count) - 1) % pageCount(count)) * kPageSize;
}
constexpr int kPagerY = 430;
constexpr unsigned kPagerDots = 6;
constexpr Rect kPreviousPage = {118, 406, 90, 50};
constexpr Rect kNextPage = {258, 406, 90, 50};
// Hub pages fit on one screen: an optional banner, a tile grid and an exit pill,
// indexed in that order. Every corner stays inside the 220px row radius.
struct Hub { bool banner; unsigned columns, tiles; int top, width, height, gap; };
constexpr unsigned kMaxHubTiles = 6;
constexpr Hub kMainHub = {true, 3, 6, 144, 118, 96, 8};
constexpr Hub kRecordsHub = {false, 2, 4, 140, 160, 84, 10};
constexpr Rect kHubBanner = {83, 78, 300, 52};
constexpr Rect kHubExit = {143, 366, 180, 46};
inline unsigned hubCount(const Hub& hub) { return (hub.banner ? 1 : 0) + hub.tiles + 1; }
inline Rect hubBounds(const Hub& hub, unsigned index) {
  if (hub.banner && index == 0) return kHubBanner;
  const unsigned tile = index - (hub.banner ? 1 : 0);
  if (tile >= hub.tiles) return kHubExit;
  const int span = int(hub.columns) * hub.width + int(hub.columns - 1) * hub.gap;
  return {kCenter - span / 2 + int(tile % hub.columns) * (hub.width + hub.gap),
          hub.top + int(tile / hub.columns) * (hub.height + hub.gap), hub.width, hub.height};
}
inline int hubAt(const Hub& hub, int x, int y) {
  for (unsigned index = 0; index < hubCount(hub); ++index)
    if (hubBounds(hub, index).contains(x, y)) return int(index);
  return -1;
}
} } }
