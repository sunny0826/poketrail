#include "platform.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>

namespace simulator {
Platform::Platform(Storage& storage, uint32_t seed) : storage_(storage), random_(seed),
    clockUtc_(storage.wallClock() + storage.clockOffset()) {}
void Platform::release() {
  keys_.fill(false);
  M5.button_a_down = M5.button_b_down = false;
  M5.Touch.detail = HostTouchDetail();
  SDL_CaptureMouse(SDL_FALSE);
  walking_ = fast_ = power_ = false;
}
void Platform::event(const SDL_Event& event) {
  if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
    release(); return;
  }
  if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
    const bool down = event.type == SDL_KEYDOWN;
    if (event.key.repeat) return;
    const auto key = event.key.keysym.scancode;
    if (key < SDL_NUM_SCANCODES) keys_[key] = down;
    M5.button_a_down = keys_[SDL_SCANCODE_LEFT] || keys_[SDL_SCANCODE_A] || keys_[SDL_SCANCODE_RETURN];
    M5.button_b_down = keys_[SDL_SCANCODE_RIGHT] || keys_[SDL_SCANCODE_D] || keys_[SDL_SCANCODE_RETURN];
    if (down && key == SDL_SCANCODE_W) { walking_ = !walking_; walkAt_ = now(); }
    if (down && key == SDL_SCANCODE_TAB) fast_ = !fast_;
    if (down && key == SDL_SCANCODE_P) power_ = true;
    if (down && key == SDL_SCANCODE_F12) screenshot_ = true;
    return;
  }
  auto& touch = M5.Touch.detail;
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
    const int x = event.button.x - kScreenX, y = event.button.y - kScreenY;
    if ((x - 233) * (x - 233) + (y - 233) * (y - 233) <= 233 * 233) {
      touch = HostTouchDetail(); touch.base_x = touch.x = x; touch.base_y = touch.y = y;
      touch.pressed = true;
      SDL_CaptureMouse(SDL_TRUE);
    }
  } else if (event.type == SDL_MOUSEMOTION && touch.pressed) {
    touch.x = event.motion.x - kScreenX; touch.y = event.motion.y - kScreenY;
  } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && touch.pressed) {
    touch.x = event.button.x - kScreenX; touch.y = event.button.y - kScreenY;
    touch.pressed = false; touch.released = true;
    touch.clicked = std::abs(touch.x - touch.base_x) <= 12 && std::abs(touch.y - touch.base_y) <= 12;
    SDL_CaptureMouse(SDL_FALSE);
  }
}
void Platform::finishFrame() { M5.Touch.detail.released = M5.Touch.detail.clicked = false; }
bool Platform::takeScreenshot() { const bool value = screenshot_; screenshot_ = false; return value; }
pokedex::ui::Sample Platform::sample() {
  pokedex::ui::Sample s;
  s.fresh = true; s.at = now(); s.z = 1;
  if (walking_) s.z -= 0.30f * std::cos(6.28318530718f * ((now() - walkAt_) % 500) / 500);
  return s;
}
pokedex::ui::Input Platform::pollInput() {
  c152::update();
  pokedex::ui::Input input;
  input.at = now(); input.power = power_; power_ = false;
  buttons_.sample(input, M5.BtnA, M5.BtnB);
  input.touch = c152::touchWasPressed();
  const auto& detail = M5.Touch.detail;
  input.x = detail.x; input.y = detail.y;
  const int dx = detail.x - detail.base_x, dy = detail.y - detail.base_y;
  input.click = detail.wasReleased() && detail.wasClicked() && std::abs(dx) <= 12 && std::abs(dy) <= 12;
  input.back = input.click && detail.base_y < 80 && detail.y < 80;
  if (detail.wasReleased() && std::abs(dx) >= 45 && std::abs(dx) > std::abs(dy))
    input.swipe = dx < 0 ? 1 : -1;
  input.touchDown = detail.isPressed();
  input.held |= input.touchDown;
  return input;
}
pokedex::device_clock::Reading Platform::readClock() {
  pokedex::device_clock::Reading reading = {};
  reading.valid = true; reading.utcSeconds = clockUtc_ + uint32_t(now() - clockAt_) / 1000;
  return reading;
}
pokedex::device_clock::Reading Platform::writeClock(int64_t utc) {
  pokedex::device_clock::Reading reading = {};
  if (!storage_.saveClock(utc)) return reading;
  clockUtc_ = utc; clockAt_ = now();
  return readClock();
}
}
