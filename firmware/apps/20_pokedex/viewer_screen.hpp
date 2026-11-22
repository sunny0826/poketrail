#pragma once
#include <cstdint>
#include "board_power.hpp"
#include "locale.hpp"
#include "menu_layout.hpp"
namespace pokedex { namespace viewer_screen {
void begin();
void update(uint32_t now);
bool render();
void invalidate();
void setLanguage(i18n::Language);
void setBattery(const c152::BatteryStatus&);
bool redraw(uint32_t now);
void paintTestBadge();
void setFeedback(void (*callback)(uint16_t, void*), void* context);
void setCollectionLabel(const char* (*callback)(uint16_t, void*), void* context);
void setEvolutionPreview(bool (*callback)(uint16_t, void*), void* context);
uint16_t species();
constexpr ui::menu_layout::Rect kEvolutionPreviewButton = {153, 402, 160, 42};
inline uint16_t nextGenerationStart(uint16_t current) {
  static constexpr uint16_t kStarts[] = {0, 151, 251, 386, 493};
  for (uint16_t start : kStarts) {
    if (start > current) return start;
  }
  return 0;
}
} }
