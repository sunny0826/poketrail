#pragma once
#include "ruins_harness.hpp"

// Dodge-first boss telegraphs on the real panel.
namespace {
// 151 and 245 start with a dodge and then alternate. The frames have to show
// that order on the real panel, not only in the catalog table: the first cue
// prompts the left key and the next cue prompts the right one. Both use the
// four-HP track and the default three-slot player bar.
void ruinsDodgeFirstBoss(uint16_t species, const char* detailFrame,
                         const char* dodgeFrame, const char* attackFrame) {
  MemoryStore store; ruinsStore(store, true, species);
  TestPlatform p; Runtime r(store, p); start(r);
  openRuins(r, p); select(r, p, 1);
  check(r.screen().page == Page::RuinsBoss, "dodge-first boss opens its detail page");
  published(r, detailFrame);
  check(ruinsHeading(speciesName(species)), "detail page keeps the boss heading");
  ruinsBossSpec("生命 我方3 / 首领4", "节拍 1.4 秒 · 12 拍",
                "节奏 闪避先手 · 判定 180/70ms");
  select(r, p, 0); select(r, p, 0);
  check(r.screen().page == Page::Battle &&
        r.engine().battleView().expected == Action::Dodge &&
        r.engine().battleView().enemyMaxHp == 4,
        "dodge-first boss starts on a dodge cue with four enemy HP");
  published(r, dodgeFrame);
  check(ruinsLine("左键 · 躲避", 401, theme::kText, true),
        "first cue prompts the left dodge key");
  check(M5.Display.readPixel(92, 110) == theme::kMint &&
        M5.Display.readPixel(110, 110) == theme::kMint &&
        M5.Display.readPixel(128, 110) == theme::kMint,
        "player HP keeps its three full segments");
  check(M5.Display.readPixel(282, 110) == theme::kGold &&
        M5.Display.readPixel(330, 110) == theme::kGold,
        "four-HP boss keeps the full 49px track");
  // Settle the dodge, then the next cue must be an attack.
  auto b = r.engine().battleView();
  while (int32_t(b.targetMs - host_time_ms) > 20) frame(r);
  p.next.left = true; p.next.held = true; frame(r, b.targetMs - host_time_ms); frame(r);
  check(r.engine().view().phase == Phase::Battle, "dodge settles and the fight continues");
  idle(r, 220);  // past the 180ms window: the next cue is now current
  check(r.engine().battleView().expected == Action::Attack, "second cue is an attack");
  idle(r, 500);  // past the 450ms effect, still before the next 1400ms cue
  published(r, attackFrame);
  check(ruinsLine("右键 · 攻击", 401, theme::kText, true),
        "second cue prompts the right attack key");
  ruinsWin(r, p);
  check(r.screen().page == Page::Throw && r.engine().view().phase == Phase::ThrowReady,
        "dodge-first boss reaches the shared throw flow");
}

void ruinsDodgeFirstBosses() {
  ruinsDodgeFirstBoss(151, "ruins-boss-151", "ruins-boss-151-dodge", "ruins-boss-151-attack");
  ruinsDodgeFirstBoss(245, "ruins-boss-245", "ruins-boss-245-dodge", "ruins-boss-245-attack");
}

// One menu row label, drawn with the same font and datum as item(). Kept as a
// separate oracle so a hidden or overwritten label cannot pass on its colour.
bool ruinsRowText(const char* text, int y, uint16_t color) {
  lgfx::LGFX_Sprite expected;
  expected.setColorDepth(16);
  if (!expected.createSprite(468, 48)) return false;
  expected.fillScreen(TFT_BLACK);
  expected.setFont(&fonts::efontCN_24);
  expected.setTextColor(color);
  expected.setTextDatum(middle_left);
  expected.drawString(text, 89, 24);
  unsigned ink = 0;
  bool same = false;
  for (int shift = -1; shift <= 1 && !same; ++shift) {
    same = true;
    for (int row = 1; row < 47 && same; ++row) for (int px = 89; px < 292; ++px) {
      const uint16_t pixel = expected.readPixel(px, row);
      if (!pixel) continue;
      ++ink;
      if (M5.Display.readPixel(px, y - 24 + row + shift) != pixel) { same = false; break; }
    }
  }
  expected.deleteSprite();
  return same && ink > 8;
}
}
