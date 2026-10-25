#pragma once
#include <cstdint>
#include "board_power.hpp"
#include "menu_layout.hpp"
namespace pokedex { namespace viewer_screen {
void begin();
void update(uint32_t now);
bool render();
void invalidate();
void setBattery(const c152::BatteryStatus&);
bool redraw(uint32_t now);
void setFeedback(void (*callback)(uint16_t, void*), void* context);
void setCollectionLabel(const char* (*callback)(uint16_t, void*), void* context);
void setEvolutionPreview(bool (*callback)(uint16_t, void*), void* context);
uint16_t species();
constexpr ui::menu_layout::Rect kEvolutionPreviewButton = {153, 402, 160, 42};
} }
