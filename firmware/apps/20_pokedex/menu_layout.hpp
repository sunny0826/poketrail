#pragma once
namespace pokedex { namespace ui { namespace menu_layout {
constexpr unsigned kPageSize = 3;
constexpr int kTop = 238;
constexpr int kMainTop = 150;
constexpr int kPitch = 68;
constexpr int kWidth = 320;
constexpr int kHeight = 60;
struct Rect {
  int x, y, width, height;
  bool contains(int px, int py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};
inline int center(unsigned row, int top = kTop) { return top + int(row) * kPitch; }
inline Rect rowBounds(unsigned row, int top = kTop) {
  return {233 - kWidth / 2, center(row, top) - kHeight / 2, kWidth, kHeight};
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
constexpr Rect kPreviousPage = {148, 410, 40, 40};
constexpr Rect kNextPage = {278, 410, 40, 40};
} } }
