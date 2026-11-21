#include "screens.hpp"
#include "board.hpp"
#include "encounter_catalog.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
namespace {
struct CircleMask {
  int16_t bg[468];
  int16_t vis[468];
};
const CircleMask& circleMask() {
  static CircleMask m;
  static bool ready = false;
  if (!ready) {
    for (int y = 0; y < 468; ++y) {
      m.bg[y] = -1;
      const int dy = y - c152::kScreenCenter;
      m.vis[y] = std::abs(dy) <= c152::kVisibleRadius
                     ? int16_t(std::sqrt(float(c152::kVisibleRadius * c152::kVisibleRadius - dy * dy)))
                     : int16_t(-1);
    }
    constexpr int32_t c = c152::kScreenCenter;
    int32_t r = 227, f = 1 - r, ddF_y = -(r << 1), ddF_x = 1, i = 0;
    m.bg[c] = int16_t(r);
    do {
      int32_t len = 0;
      while (f < 0) { f += (ddF_x += 2); ++len; }
      i += len;
      f += (ddF_y += 2);
      if (len) {
        for (int32_t row = c + i - len + 1; row <= c + i; ++row)
          m.bg[row] = std::max<int16_t>(m.bg[row], int16_t(r));
        for (int32_t row = c - i; row <= c - i + len - 1; ++row)
          m.bg[row] = std::max<int16_t>(m.bg[row], int16_t(r));
      }
      m.bg[c + r] = std::max<int16_t>(m.bg[c + r], int16_t(i));
      m.bg[c - r] = std::max<int16_t>(m.bg[c - r], int16_t(i));
    } while (i < --r);
    ready = true;
  }
  return m;
}
}
void Screens::clearRegion(int x, int y, int w, int h, uint16_t bg) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 468) w = 468 - x;
  if (y + h > 468) h = 468 - y;
  if (w <= 0 || h <= 0) return;
  const auto& m = circleMask();
  if (y >= 6 && y + h <= 461) {
    const int minHs = std::min(m.bg[y], m.bg[y + h - 1]);
    if (x >= c152::kScreenCenter - minHs && x + w <= c152::kScreenCenter + minHs + 1) {
      M5.Display.fillRect(x, y, w, h, bg);
      return;
    }
  }
  const int xEnd = x + w;
  for (int row = y; row < y + h; ++row) {
    const int hs = m.bg[row], vs = m.vis[row];
    if (hs >= 0) {
      const int bx0 = std::max(x, c152::kScreenCenter - hs);
      const int bx1 = std::min(xEnd, c152::kScreenCenter + hs + 1);
      if (bx0 < bx1) M5.Display.fillRect(bx0, row, bx1 - bx0, 1, bg);
      if (vs > hs) {
        const int lx0 = std::max(x, c152::kScreenCenter - vs);
        const int lx1 = std::min(xEnd, c152::kScreenCenter - hs);
        if (lx0 < lx1) M5.Display.fillRect(lx0, row, lx1 - lx0, 1, TFT_BLACK);
        const int rx0 = std::max(x, c152::kScreenCenter + hs + 1);
        const int rx1 = std::min(xEnd, c152::kScreenCenter + vs + 1);
        if (rx0 < rx1) M5.Display.fillRect(rx0, row, rx1 - rx0, 1, TFT_BLACK);
      }
    } else if (vs >= 0) {
      const int vx0 = std::max(x, c152::kScreenCenter - vs);
      const int vx1 = std::min(xEnd, c152::kScreenCenter + vs + 1);
      if (vx0 < vx1) M5.Display.fillRect(vx0, row, vx1 - vx0, 1, TFT_BLACK);
    }
  }
}
void Screens::restoreSceneRegion(int x, int y, int w, int h, Route route, bool dim) {
  const int y0 = std::max(y, 92), y1 = std::min(y + h, 362);
  const int x0 = std::max(x, 0), x1 = std::min(x + w, 468);
  if (x0 >= x1 || y0 >= y1) return;
  constexpr int kSceneW = data::kSceneWidth * data::kSceneScale;
  const unsigned idx = isRegion(route) ? regionIndex(route) : 0;
  const auto& src = dim ? data::kRegionScenesDim[idx] : data::kRegionScenes[idx];
  if (sceneCache_ && (!sceneCacheValid_ || sceneCacheRoute_ != route || sceneCacheDim_ != dim)) {
    for (unsigned sy = 0; sy < data::kSceneHeight; ++sy) {
      const uint16_t* row = src.pixels + sy * data::kSceneWidth;
      uint16_t* dst = sceneCache_ + size_t(sy * data::kSceneScale) * kSceneW;
      for (unsigned sx = 0; sx < data::kSceneWidth; ++sx) {
        const uint16_t px = sceneEdgeVisible(sx, sy) ? row[sx] : theme::kBackground;
        dst[sx * 2] = dst[sx * 2 + 1] = px;
      }
      std::memcpy(dst + kSceneW, dst, kSceneW * sizeof(uint16_t));
    }
    sceneCacheRoute_ = route;
    sceneCacheDim_ = dim;
    sceneCacheValid_ = true;
  }
  const auto& m = circleMask();
  const int minVis = std::min(m.vis[y0], m.vis[y1 - 1]);
  if (sceneCache_ && x0 >= 5 && x1 <= 461 && x0 >= c152::kScreenCenter - minVis &&
      x1 <= c152::kScreenCenter + minVis + 1) {
    M5.Display.setClipRect(x0, y0, x1 - x0, y1 - y0);
    M5.Display.pushImage(5, y0, kSceneW, y1 - y0, sceneCache_ + size_t(y0 - 92) * kSceneW);
    M5.Display.clearClipRect();
    return;
  }
  for (int row = y0; row < y1; ++row) {
    const int vis = m.vis[row];
    const int sx0 = std::max(x0, std::max(5, c152::kScreenCenter - vis));
    const int sx1 = std::min(x1, std::min(461, c152::kScreenCenter + vis + 1));
    if (sx0 < sx1) {
      const uint16_t* ptr = sceneCache_ ? sceneCache_ + size_t(row - 92) * kSceneW : sceneRow_;
      if (!sceneCache_) {
        const unsigned sy = unsigned(row - 92) / data::kSceneScale;
        const uint16_t* srow = src.pixels + sy * data::kSceneWidth;
        for (unsigned sx = 0; sx < data::kSceneWidth; ++sx)
          sceneRow_[sx * 2] = sceneRow_[sx * 2 + 1] = sceneEdgeVisible(sx, sy) ? srow[sx] : theme::kBackground;
      }
      M5.Display.setClipRect(sx0, row, sx1 - sx0, 1);
      M5.Display.pushImage(5, row, kSceneW, 1, ptr);
      M5.Display.clearClipRect();
    }
    if (x0 < sx0) clearRegion(x0, row, sx0 - x0, 1, theme::kBackground);
    if (sx1 < x1) clearRegion(sx1, row, x1 - sx1, 1, theme::kBackground);
  }
}
void Screens::restoreBackdrop(int x, int y, int w, int h, Route route, bool dim, bool hasScene) {
  if (!hasScene) { clearRegion(x, y, w, h, theme::kBackground); return; }
  if (y < 92) clearRegion(x, y, w, std::min(y + h, 92) - y, theme::kBackground);
  const int sy0 = std::max(y, 92), sy1 = std::min(y + h, 362);
  if (sy0 < sy1) restoreSceneRegion(x, sy0, w, sy1 - sy0, route, dim);
  if (y + h > 362) clearRegion(x, std::max(y, 362), w, y + h - std::max(y, 362), theme::kBackground);
}
void Screens::practiceThrow(const ScreenState& s, uint32_t now) {
  scene(regionOf(s.practiceSpecies), true);
  title(speciesName(s.practiceSpecies), "", theme::kGold);
  if (s.capture.active) { capture(s, s.practiceSpecies, now); return; }
  ballSlots(s.practiceUsedBalls);
  if (s.practiceThrowing) {
    aim(s.practiceSpecies, now, true, int32_t(s.practiceTargetMs - now));
    line(tr(Text::ReleaseThrowCost), 399, theme::kMint);
    return;
  }
  aim(s.practiceSpecies, now, false, 0);
  line(tr(Text::HoldThrow), 399, theme::kMint);
  line(tr(Text::EndPracticeTop), 426, theme::kMuted);
}
void Screens::practiceResult(const ScreenState& s, uint32_t now) {
  title(s.practiceResult == Result::Captured ? tr(Text::PracticeSuccess) :
        s.practiceResult == Result::Lost ? tr(Text::BattleLost) : tr(Text::PracticeEnded),
        speciesName(s.practiceSpecies), theme::kGold);
  if (s.practiceResult == Result::Captured) {
    actorShadow(233, 168);
    sprite(s.practiceSpecies, now, resultPlayback_, 168, 233, 0, 2, 120);
    line(tr(Text::NotCollected), 262, theme::kMuted);
  } else {
    thumbnail(s.practiceSpecies, 233, 160, 2);
    if (s.practiceResult != Result::Lost) line(tr(Text::PokemonFled), 240, theme::kMuted);
  }
  item(s, 0, tr(Text::PracticeAgain), menu_layout::kTop);
  item(s, 1, tr(Text::ReturnExploration), menu_layout::kTop);
}
void Screens::drawPracticeResultRow(const ScreenState& s, const Engine&, unsigned index, int top) {
  item(s, index, index == 0 ? tr(Text::PracticeAgain) : tr(Text::ReturnExploration), top);
}
void Screens::drawPractice(const ScreenState& s, const Engine& e, const BattleView&, uint32_t now, bool full) {
  if (s.page == Page::PracticeThrow) {
    const Route route = regionOf(s.practiceSpecies);
    const int32_t remaining = s.practiceThrowing ? int32_t(s.practiceTargetMs - now) : 0;
    if (!full) {
      drawThrowStagePartial(s, route, s.practiceSpecies, s.practiceThrowing, remaining,
                            false, s.practiceUsedBalls, false, now, true);
      return;
    }
    practiceThrow(s, now);
    bool isBurst = false;
    uint8_t promptKind = s.practiceThrowing ? 1 : 5;
    if (s.capture.active) {
      const auto f = captureFrameAt(s.capture, now);
      isBurst = f.stage == CaptureStage::Reveal && !s.capture.captured;
      promptKind = f.stage == CaptureStage::Reveal ? (s.capture.captured ? 3 : 4) : 0;
    }
    snap_.route = route;
    snap_.species = s.practiceSpecies;
    snap_.legacyNoBalls = false;
    snap_.showBallSlots = !s.capture.active;
    snap_.usedBalls = s.practiceUsedBalls;
    snap_.captureActive = s.capture.active;
    snap_.prevCaptureBurst = isBurst;
    snap_.throwPromptKind = promptKind;
    snap_.timingActive = s.practiceThrowing;
    snap_.throwCommitting = false;
    snap_.spriteRect = spriteBounds(s.practiceSpecies, 233, 233, 2, 190);
    snap_.animFrame = s.practiceSpecies ? battlePlayback_.state.frame_index : UINT16_MAX;
    return;
  }
  if (full) {
    practiceResult(s, now);
  } else if (s.practiceSpecies != snap_.species || s.practiceResult != snap_.result) {
    WriteScope sc;
    clearRegion(0, 36, 468, 405, theme::kBackground);
    practiceResult(s, now);
  } else {
    if (s.practiceResult == Result::Captured && s.practiceSpecies) {
      advance(resultPlayback_, s.practiceSpecies, now);
      if (resultPlayback_.state.frame_index != snap_.animFrame) {
        WriteScope sc;
        clearRegion(72, 100, 324, 108, theme::kBackground);
        M5.Display.setClipRect(72, 100, 324, 108);
        sprite(s.practiceSpecies, now, resultPlayback_, 168, 233, 0, 2, 120);
        M5.Display.clearClipRect();
      }
    }
    updateListRows(s, e, menu_layout::kTop, false, 2, &Screens::drawPracticeResultRow);
  }
  snap_.species = s.practiceSpecies;
  snap_.result = s.practiceResult;
  snap_.animFrame = s.practiceResult == Result::Captured && s.practiceSpecies
                        ? resultPlayback_.state.frame_index : UINT16_MAX;
}
} }
