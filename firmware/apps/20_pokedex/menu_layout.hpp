#pragma once
namespace pokedex { namespace ui { namespace menu_layout {
constexpr unsigned kPageSize = 3;
constexpr int kTop = 238;
constexpr int kMainTop = 150;
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
} } }
