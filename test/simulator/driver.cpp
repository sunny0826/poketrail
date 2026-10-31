#include "driver.hpp"
#include "../../simulator/app.hpp"
#include <stdexcept>
#include <unistd.h>

namespace simulator { namespace e2e {
namespace {
std::string json(const std::string& s) {
  std::string out;
  for (char c : s) { if (c == '"' || c == '\\') out += '\\'; if (c == '\n') out += "\\n"; else out += c; }
  return out;
}
}
Driver::Driver(const std::string& saveDir, const std::string& output, uint32_t seed, int64_t utc)
    : output_(output) {
  events_ = std::fopen((output + "/events.jsonl").c_str(), "w");
  if (!events_) throw std::runtime_error("cannot create events");
  Options options; options.seed = seed; options.isolateHostFocus = true;
  options.wallClock = [utc] { return utc + host_time_ms / 1000; };
  options.error = [this](const std::string& message) { ++closeErrors_; event("close-error", message); };
  options.windowEvent = [this](unsigned value) { event("window-event", std::to_string(value)); };
  app_.reset(new App(saveDir, output, options));
  event("video-driver", SDL_GetCurrentVideoDriver());
  frames();
}
Driver::~Driver() { app_.reset(); if (events_) std::fclose(events_); }
const Runtime& Driver::runtime() const { return app_->runtime(); }
uint32_t Driver::now() const { return host_time_ms; }
void Driver::event(const std::string& kind, const std::string& detail) {
  std::fprintf(events_, "{\"at\":%u,\"kind\":\"%s\",\"detail\":\"%s\"}\n", now(), json(kind).c_str(), json(detail).c_str());
  std::fflush(events_);
}
void Driver::check(bool ok, const std::string& message) {
  event(ok ? "pass" : "fail", message);
  if (!ok) throw std::runtime_error(message);
}
void Driver::frames(unsigned count) {
  for (unsigned i = 0; i < count; ++i) if (!app_->step()) check(false, "event loop closed unexpectedly");
}
void Driver::advance(uint32_t ms) {
  const uint32_t start = now(); while (now() - start < ms) frames(1);
}
void Driver::until(const std::function<bool()>& predicate, uint32_t maxMs, const std::string& message) {
  const uint32_t start = now();
  while (!predicate() && now() - start < maxMs) frames(1);
  check(predicate(), message);
}
void Driver::push(SDL_Event& e, unsigned ticks) {
  check(SDL_PushEvent(&e) == 1, "SDL accepts event"); frames(ticks);
}
void Driver::key(SDL_Scancode code, bool down, unsigned ticks) {
  event(down ? "key-down" : "key-up", std::to_string(code));
  SDL_Event e = {}; e.type = down ? SDL_KEYDOWN : SDL_KEYUP; e.key.keysym.scancode = code; push(e, ticks);
}
void Driver::press(SDL_Scancode code) { key(code, true); key(code, false); }
void Driver::mouse(int x, int y, bool down) {
  event(down ? "touch-down" : "touch-up", std::to_string(x) + "," + std::to_string(y));
  SDL_Event e = {}; e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  e.button.button = SDL_BUTTON_LEFT; e.button.x = x + kScreenX; e.button.y = y + kScreenY; push(e);
}
void Driver::tap(int x, int y) { mouse(x, y, true); mouse(x, y, false); }
void Driver::swipe(int fromX, int toX, int y) {
  mouse(fromX, y, true);
  for (int i = 1; i <= 4; ++i) {
    SDL_Event e = {}; e.type = SDL_MOUSEMOTION; e.motion.x = fromX + (toX-fromX)*i/4 + kScreenX;
    e.motion.y = y + kScreenY; push(e, 1);
  }
  mouse(toX, y, false);
}
void Driver::select(unsigned index) {
  frames();
  const unsigned count = menuCount(screen(), runtime().engine());
  check(index < count, "menu index in range");
  for (unsigned i = 0; screen().selected != index && i < count; ++i) press(SDL_SCANCODE_RIGHT);
  check(screen().selected == index, "menu selected " + std::to_string(index)); press(SDL_SCANCODE_RETURN);
}
void Driver::back() { tap(233, 35); }
void Driver::wake() { if (dimmed()) press(SDL_SCANCODE_P); }
void Driver::homeMenu() {
  wake();
  for (unsigned i = 0; screen().page != Page::Home && screen().page != Page::Menu && i < 8; ++i) back();
  if (screen().page == Page::Home) press(SDL_SCANCODE_RIGHT);
  check(screen().page == Page::Menu, "home menu visible");
}
void Driver::walk(unsigned steps) {
  const auto target = save().totalSteps + steps;
  press(SDL_SCANCODE_W); press(SDL_SCANCODE_TAB);
  until([&] { return save().totalSteps >= target; }, steps * 600 + 20000, "walking counted by detector");
  press(SDL_SCANCODE_W); press(SDL_SCANCODE_TAB); advance(200); wake();
}
void Driver::focusLost(bool hostWindow) {
  event("focus-lost", hostWindow ? "host-window" : "synthetic");
  SDL_Event e = {}; e.type = SDL_WINDOWEVENT;
  e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
  e.window.windowID = hostWindow ? app_->windowId() : 0;
  push(e);
  if (!hostWindow)
    check(!M5.button_a_down && !M5.button_b_down && !M5.Touch.detail.pressed, "focus releases controls");
}
int64_t Driver::rtcUtc() const { return app_->platform().readClock().utcSeconds; }
void Driver::close(Close expected) {
  event("quit", "SDL_QUIT"); SDL_Event e = {}; e.type = SDL_QUIT;
  check(SDL_PushEvent(&e) == 1, "SDL accepts quit");
  bool open = true; const auto start = now(); const unsigned errors = closeErrors_;
  while (open && closeErrors_ == errors && now() - start < 10000) open = app_->step();
  check(expected == Close::Closed ? !open && closeErrors_ == errors : open && closeErrors_ > errors,
        expected == Close::Closed ? "normal close succeeds" : "failed close keeps the window open");
  event("close-result", open ? "protected" : "closed");
}
void Driver::fault(Fault value) { event("fault", std::to_string(int(value))); app_->fault(value); }
unsigned Driver::faultHits() const { return app_->faultHits(); }
unsigned Driver::writes() const { return app_->writes(); }
void Driver::freeze() {
  state("frozen"); shot("frozen"); event("kill-ready", "Runtime and logical clock stopped; lock retained");
  FILE* ready = std::fopen((output_ + "/ready.fifo").c_str(), "w");
  if (!ready) throw std::runtime_error("kill handshake unavailable");
  std::fputs("kill-ready\n", ready); std::fclose(ready);
  for (;;) ::pause();
}
void Driver::shot(const std::string& name) {
  app_->draw(); app_->screenshot(name); event("screenshot", name);
}
} }
