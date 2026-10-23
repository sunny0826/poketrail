#pragma once

// M5Stack StopWatch (C152) 的板级初始化与共享工具。
//
// 屏幕初始化、输入读取与震动反馈；玩法和页面状态由应用 Runtime 管理。

#include <Arduino.h>
#include <M5Unified.h>

namespace c152 {

// 采用 466x466 的应用坐标，圆心 (233,233)，安全可视半径 228。
// M5GFX 0.2.27 的 StopWatch 驱动报告 468x468；界面避开边缘差异。
constexpr int32_t kScreenSize = 466;
constexpr int32_t kScreenCenter = 233;
constexpr int32_t kVisibleRadius = 228;

// 初始化屏幕、电源、IMU 与音频。RTC 由应用启动。每次只能调用一次。
void begin(const char* title);

// 每帧调用一次，刷新 M5Unified 的输入与电源状态。
void update();

// 触摸的上升沿（按下瞬间）。返回 true 表示这一帧刚按下。
bool touchWasPressed();

// 震动反馈 + 声音。frequency_hz 为 0 时只震动。
void feedback(uint16_t frequency_hz, uint16_t duration_ms, uint8_t vibration);
bool vibrationActive();

// 在圆屏底部画一行提示文字。
void drawFooter(const char* text, uint16_t color);

}  // namespace c152
