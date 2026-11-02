#pragma once
#include "board.hpp"
#include "menu_layout.hpp"
#include "screen_theme.hpp"

namespace round_layout_test {
using pokedex::ui::menu_layout::Rect;
inline bool background(int x, int y) {
  return M5.Display.readPixel(x, y) == pokedex::ui::theme::kBackground;
}
// Text and artwork never touch or slide under a list button.
inline bool clearAbove(const Rect& row, int gap) {
  for (int y = row.y - gap; y < row.y; ++y)
    for (int x = row.x; x < row.x + row.width; ++x)
      if (!background(x, y)) return false;
  return true;
}
// The painted surface of a row ends exactly at its touch bounds.
inline bool rowEdges(const Rect& row) {
  const int y = row.y + row.height / 2;
  return !background(row.x + 6, y) && !background(row.x + row.width - 7, y) &&
         background(row.x - 1, y) && background(row.x + row.width, y);
}
// Rows are stadium-shaped: corners that a 16px rounded rectangle would fill stay empty.
inline bool pillRow(const Rect& row) {
  const int right = row.x + row.width - 1, bottom = row.y + row.height - 1;
  return background(row.x + 10, row.y + 6) && background(right - 10, row.y + 6) &&
         background(row.x + 10, bottom - 6) && background(right - 10, bottom - 6) &&
         !background(row.x + row.height / 2, row.y + 3) && !background(row.x + 6, row.y + row.height / 2);
}
// Short lists show one dot per page; the current page is larger and uses the page accent.
inline bool pagerDots(unsigned pages, unsigned page, uint16_t accent) {
  const int y = pokedex::ui::menu_layout::kPagerY, first = 233 - int(pages - 1) * 7;
  for (unsigned i = 0; i < pages; ++i) {
    const int x = first + int(i) * 14;
    const uint16_t center = M5.Display.readPixel(x, y), rim = M5.Display.readPixel(x + 3, y);
    if (i == page ? center != accent || rim != accent
                  : center != pokedex::ui::theme::kTrack || !background(x + 3, y)) return false;
  }
  return background(233 - int(pages) * 7 - 4, y) && background(168 + 14, y) && background(298 - 14, y);
}
inline bool insideRowRadius(const Rect& row) {
  const int r = pokedex::ui::menu_layout::kRowRadius;
  for (int y : {row.y, row.y + row.height - 1})
    for (int x : {row.x, row.x + row.width - 1}) {
      const int dx = x - pokedex::ui::menu_layout::kCenter, dy = y - pokedex::ui::menu_layout::kCenter;
      if (dx * dx + dy * dy > r * r) return false;
    }
  return true;
}
}
