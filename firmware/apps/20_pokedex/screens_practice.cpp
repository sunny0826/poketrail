#include "screens.hpp"
#include "board.hpp"
#include <cstdio>
namespace pokedex { namespace ui {
using namespace adventure;
void Screens::practiceThrow(const ScreenState& s, uint32_t now) {
  scene(Route::Forest, true);
  if (s.practiceThrowing) title(speciesName(s.practiceSpecies), "捕获练习 · 不消耗精灵球", theme::kGold);
  else {
    char text[32];
    snprintf(text, sizeof(text), "本场剩余 %u 次", 2 - s.practiceUsedBalls);
    title(speciesName(s.practiceSpecies), text, theme::kGold);
  }
  if (s.capture.active) { capture(s, s.practiceSpecies, now); return; }
  if (s.practiceThrowing) {
    aim(s.practiceSpecies, now, true, int32_t(s.practiceTargetMs - now), s.charging, now - s.chargeStartMs);
    line("按住蓄力 · 松手投球", 399, theme::kMint);
    line("成功不会收入图鉴", 426, theme::kMuted);
    return;
  }
  aim(s.practiceSpecies, now, false, 0, false, 0);
  line("按住右键蓄力 · 松手投球", 399, theme::kMint);
  line("轻触顶部结束练习", 426, theme::kMuted);
}
void Screens::practiceResult(const ScreenState& s, uint32_t now) {
  title(s.practiceResult == Result::Captured ? "练习成功" :
        s.practiceResult == Result::Lost ? "对战失败" : "练习结束",
        speciesName(s.practiceSpecies), theme::kGold);
  if (s.practiceResult == Result::Captured) {
    actorShadow(233, 168);
    sprite(s.practiceSpecies, now, resultPlayback_, 168, 233, 0, 2, 120);
    line("未收入图鉴", 262, theme::kMuted);
  } else {
    thumbnail(s.practiceSpecies, 233, 160, 2);
    line(s.practiceResult == Result::Lost ? "对战失败 · 下次再试" : "宝可梦逃走了", 240, theme::kMuted);
  }
  item(s, 0, "再练一只", menu_layout::kTop);
  item(s, 1, "返回探索", menu_layout::kTop);
}
} }
