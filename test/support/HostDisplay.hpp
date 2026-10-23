#pragma once

#include <lgfx/v1/LGFX_Sprite.hpp>
#include <vector>

// 使用真实像素绘制和 IPanel 事务，模拟 StopWatch 的自动刷新开关。
// 每次 display 保留当时的画面，便于检测擦除后、重画前暴露的中间帧。
class HostDisplay : public lgfx::LGFXBase {
 public:
  HostDisplay() {
    _panel = &panel_;
    setColorDepth(16);
  }
  ~HostDisplay() { panel_.deleteSprite(); }

  void* createSprite(int32_t width, int32_t height) {
    void* buffer = panel_.createSprite(width, height, &_write_conv, false);
    setRotation(0);
    return buffer;
  }
  void* getBuffer() { return panel_.getBuffer(); }
  void setBrightness(uint8_t value) { brightness = value; }
  uint8_t brightness = 96;

  void beginObservation() {
    panel_.frames.clear();
    panel_.observing = true;
  }
  void endObservation() { panel_.observing = false; }
  const std::vector<std::vector<uint16_t>>& publishedFrames() const {
    return panel_.frames;
  }
  std::vector<uint16_t> framebuffer() {
    const auto* buffer = static_cast<const uint16_t*>(getBuffer());
    return {buffer, buffer + width() * height()};
  }

 private:
  struct ObservedPanel : lgfx::Panel_Sprite {
    ObservedPanel() {
      _start_count = 0;
      setAutoDisplay(true);
    }
    void display(uint_fast16_t, uint_fast16_t,
                 uint_fast16_t, uint_fast16_t) override {
      if (!observing) return;
      const auto* buffer = static_cast<const uint16_t*>(getBuffer());
      frames.emplace_back(buffer, buffer + width() * height());
    }
    bool observing = false;
    std::vector<std::vector<uint16_t>> frames;
  } panel_;
};
