#pragma once

#include <Arduino.h>
#include <M5Unified.h>

namespace c152 {

// M5GFX 0.2.27 的 StopWatch 驱动报告 468x468；界面避开边缘差异。
constexpr int32_t kScreenSize = 466;
constexpr int32_t kScreenCenter = 233;
constexpr int32_t kVisibleRadius = 228;

void begin(const char* title);

void update();
void updateVibration();

bool restUntilInput(uint32_t durationMs);

bool touchWasPressed();
void resetTouchEdge();
bool readAccel(float& x, float& y, float& z, bool& fresh);

void feedback(uint16_t frequency_hz, uint16_t duration_ms, uint8_t vibration);
bool vibrationActive();

void drawFooter(const char* text, uint16_t color);

}
