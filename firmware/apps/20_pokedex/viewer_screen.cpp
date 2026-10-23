#include <Arduino.h>
#include <M5Unified.h>
#include <stdlib.h>

#include "board.hpp"
#include "viewer_screen.hpp"
#include "game.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
#include "battery_screen.hpp"

namespace pokedex { namespace viewer_screen {

using pokedex::viewer::State;
using pokedex::viewer::Input;

State g_state;
uint16_t* g_pixels = nullptr;
constexpr size_t kPixelCapacity = size_t(pokedex::kMaxDisplayWidth) * pokedex::kMaxDisplayHeight;
constexpr int32_t kFieldTop = 84;
constexpr int32_t kFieldBottom = 374;
uint16_t g_decoded_sprite = UINT16_MAX;
uint16_t g_decoded_frame = UINT16_MAX;
uint16_t g_label_sprite = UINT16_MAX;
bool g_screen_drawn = false;
bool g_ready = false;
c152::BatteryStatus g_battery;
bool g_battery_dirty = false;
void (*feedbackCallback)(uint16_t, void*) = nullptr;
void* callbackContext = nullptr;
const char* (*collectionCallback)(uint16_t, void*) = nullptr;
void* collectionContext = nullptr;

void text(const char* value, int32_t y, uint16_t color, const lgfx::IFont* font) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(font);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(value, c152::kScreenCenter, y);
}

void drawLabels(const pokedex::Sprite& sprite) {
  M5.Display.fillRect(0, 0, c152::kScreenSize, kFieldTop, TFT_BLACK);
  M5.Display.fillRect(0, kFieldBottom, c152::kScreenSize,
                      c152::kScreenSize - kFieldBottom, TFT_BLACK);
  char label[24];
  snprintf(label, sizeof(label), "#%03u / %03u", unsigned(sprite.id),
           unsigned(pokedex::data::kSpriteCount));
  text(label, 34, TFT_DARKGREY, &fonts::Font2);
  text(sprite.name, 65, TFT_WHITE, &fonts::efontCN_24);
  text(sprite.types, 390, TFT_CYAN, &fonts::efontCN_16);
  char footer[96];
  if (collectionCallback) {
    snprintf(footer, sizeof(footer), "%s · 顶部返回", collectionCallback(sprite.id, collectionContext));
    text(footer, 419, TFT_DARKGREY, &fonts::efontCN_16);
  } else text("互动 · 按键切换 · 顶部返回", 419, TFT_DARKGREY, &fonts::efontCN_16);
}

void drawHeart() {
  constexpr uint16_t pink = 0xF81B;
  constexpr int32_t x = c152::kScreenCenter + 182;
  M5.Display.fillCircle(x - 6, 194, 6, pink);
  M5.Display.fillCircle(x + 6, 194, 6, pink);
  M5.Display.fillTriangle(x - 12, 196, x + 12, 196, x, 209, pink);
}

bool render() {
  const auto& sprite = pokedex::data::kSprites[g_state.sprite_index];
  const uint16_t scale = pokedex::displayScale(sprite);
  if (g_decoded_sprite != g_state.sprite_index || g_decoded_frame != g_state.frame_index) {
    if (!pokedex::decodeFrame(sprite, g_state.frame_index, scale, g_pixels, kPixelCapacity)) {
      Serial.printf("animation decode failed: id=%u frame=%u\n", sprite.id, g_state.frame_index);
      return false;
    }
    g_decoded_sprite = g_state.sprite_index;
    g_decoded_frame = g_state.frame_index;
  }
  // StopWatch在最外层endWrite自动刷新；擦除和重画只能一起提交。
  M5.Display.startWrite();
  if (!g_screen_drawn) {
    M5.Display.fillScreen(TFT_BLACK);
    g_screen_drawn = true;
  }
  if (g_label_sprite != g_state.sprite_index) {
    drawLabels(sprite);
    g_label_sprite = g_state.sprite_index;
  }
  M5.Display.fillRect(0, kFieldTop, c152::kScreenSize,
                      kFieldBottom - kFieldTop, TFT_BLACK);
  const int32_t width = sprite.width * scale;
  const int32_t height = sprite.height * scale;
  M5.Display.pushImage(c152::kScreenCenter - width / 2,
                      c152::kScreenCenter - height / 2 - g_state.jump,
                      width, height, g_pixels, pokedex::kTransparentKey);
  if (g_state.reacting) drawHeart();
  if (collectionCallback) ui::drawBattery(g_battery);
  g_battery_dirty = false;
  M5.Display.endWrite();
  return true;
}

void begin() {
  if (!g_pixels) g_pixels = static_cast<uint16_t*>(malloc(kPixelCapacity * sizeof(uint16_t)));
  if (!g_pixels) {
    Serial.printf("animation framebuffer allocation failed\n");
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_BLACK);
    text("动画内存不足", c152::kScreenCenter, TFT_RED, &fonts::efontCN_16);
    M5.Display.endWrite();
    return;
  }
  g_state.animation_started_ms = millis();
  g_state.frame_index = 0;
  g_ready = render();
  Serial.printf("viewer ready=%u animations=%u first=#%03u frames=%u\n",
                unsigned(g_ready), unsigned(pokedex::data::kSpriteCount),
                pokedex::data::kSprites[0].id, pokedex::data::kSprites[0].frame_count);
}

void update(uint32_t now) {
  if (!g_ready) { delay(20); return; }
  Input input = Input::None;
  const char* source = "swipe";
  const auto& detail = M5.Touch.getDetail();
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
    if (M5.BtnA.isPressed() != M5.BtnB.isPressed()) {
      input = M5.BtnA.wasPressed() ? Input::Previous : Input::Next;
      source = M5.BtnA.wasPressed() ? "left-button" : "right-button";
    }
  } else if (detail.wasReleased()) {
    input = pokedex::viewer::releaseInput(
        detail.base_x, detail.base_y, detail.x, detail.y, detail.wasClicked());
    if (input == Input::React) source = "react";
  }
  bool dirty = pokedex::viewer::applyInput(g_state, input, pokedex::data::kSpriteCount, now);
  if (dirty) {
    if (feedbackCallback) feedbackCallback(input == Input::React ? 55 : 25, callbackContext);
    Serial.printf("input=%s id=%u\n", source, pokedex::data::kSprites[g_state.sprite_index].id);
  }
  dirty = pokedex::viewer::update(g_state, pokedex::data::kSprites[g_state.sprite_index], now) || dirty;
  static uint32_t frames = 0;
  if (dirty || g_battery_dirty || !g_screen_drawn) {
    g_ready = render();
    ++frames;
  }
  static uint32_t report_ms = 0;
  if (now - report_ms >= 2000) {
    Serial.printf("viewer id=%u frame=%u rendered=%lu ready=%u\n",
                  pokedex::data::kSprites[g_state.sprite_index].id, g_state.frame_index,
                  static_cast<unsigned long>(frames), unsigned(g_ready));
    frames = 0;
    report_ms = now;
  }
}

void invalidate() { g_screen_drawn = false; g_label_sprite = UINT16_MAX; }
bool redraw(uint32_t now) {
  if (!g_ready) return false;
  pokedex::viewer::update(g_state, pokedex::data::kSprites[g_state.sprite_index], now);
  invalidate();
  g_ready = render();
  return g_ready;
}
void setBattery(const c152::BatteryStatus& status) {
  if (g_battery.percent == status.percent && g_battery.charge == status.charge) return;
  g_battery = status;
  g_battery_dirty = true;
}
void setFeedback(void (*callback)(uint16_t, void*), void* context) {
  feedbackCallback = callback; callbackContext = context;
}
void setCollectionLabel(const char* (*callback)(uint16_t, void*), void* context) {
  collectionCallback = callback; collectionContext = context;
  g_label_sprite = UINT16_MAX;
}
} }
