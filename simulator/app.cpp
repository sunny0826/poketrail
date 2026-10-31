#include "app.hpp"
#include <cstdio>
#include <stdexcept>

uint32_t host_time_ms = 0;
HostSerial Serial;
HostM5 M5;

namespace simulator {
namespace {
void require(bool ok) { if (!ok) throw std::runtime_error(SDL_GetError()); }
}
App::App(const std::string& saveDir, const std::string& screenshots, const Options& options)
    : options_(options), storage_(saveDir, options), io_(storage_), store_(io_), platform_(storage_, options.seed),
      pixels_(kWidth * kHeight * 3), screenshots_(screenshots) {
  require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) == 0);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  window_ = SDL_CreateWindow("PokeTrail · 桌面模拟", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            kWidth, kHeight, SDL_WINDOW_SHOWN);
  require(window_ != nullptr);
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
  require(renderer_ != nullptr);
  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
  require(texture_ != nullptr);
  footer_.setColorDepth(16);
  if (!footer_.createSprite(kWidth, kHeight - 504)) throw std::runtime_error("无法分配控制提示画布");
  c152::begin("PokeTrail Simulator");
  if (!M5.Display.getBuffer()) throw std::runtime_error("无法分配圆屏画布");
  runtime_.reset(new pokedex::ui::Runtime(store_, platform_));
  if (!runtime_->begin()) throw std::runtime_error("无法初始化 Runtime 画面");
  std::printf("模拟存档：%s\n截图目录：%s\n", saveDir.c_str(), screenshots.c_str());
}
App::~App() {
  runtime_.reset();
  footer_.deleteSprite();
  SDL_DestroyTexture(texture_); SDL_DestroyRenderer(renderer_); SDL_DestroyWindow(window_);
  SDL_Quit();
}
bool App::step() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_WINDOWEVENT && options_.windowEvent) options_.windowEvent(event.window.event);
    // Scripted focus events use windowID 0; normal desktop mode handles all focus changes.
    if (options_.isolateHostFocus && event.type == SDL_WINDOWEVENT && event.window.windowID != 0 &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) continue;
    if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) ||
        (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_Q &&
         (event.key.keysym.mod & KMOD_GUI))) {
      closing_ = true;
      runtime_->engine().requestPause();
      SDL_Event lost = {}; lost.type = SDL_WINDOWEVENT; lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
      platform_.event(lost);
      break;
    }
    platform_.event(event);
    // Consume input edges on separate ticks, including down/up in one SDL batch.
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
        event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
        (event.type == SDL_MOUSEMOTION && M5.Touch.detail.pressed)) break;
  }
  if (!running_) return false;
  for (unsigned i = 0, count = platform_.fast() ? 16 : 1; i < count; ++i) {
    host_time_ms += 20; runtime_->loop(); platform_.finishFrame();
  }
  if (runtime_->takeTestRequest() != pokedex::ui::Runtime::TestRequest::None)
    runtime_->openTestPage(pokedex::ui::Page::TestEntry, true);
  draw();
  if (closing_ && !(runtime_->engine().battleView().active && !runtime_->engine().battleView().paused) &&
      !runtime_->engine().throwView().active) {
    if (close()) running_ = false;
    closing_ = false;
  }
  if (platform_.takeScreenshot()) {
    try { screenshot("capture-" + std::to_string(++shots_)); }
    catch (const std::exception& error) { std::fprintf(stderr, "截图失败：%s\n", error.what()); }
  }
  return running_;
}
void App::draw() {
  std::fill(pixels_.begin(), pixels_.end(), uint8_t(18));
  std::vector<uint8_t> screen(kScreenSize * kScreenSize * 3);
  if (!runtime_->dimmed()) M5.Display.readRectRGB(0, 0, kScreenSize, kScreenSize, screen.data());
  for (int y = 0; y < kScreenSize; ++y) for (int x = 0; x < kScreenSize; ++x) {
    if ((x - 233) * (x - 233) + (y - 233) * (y - 233) > 233 * 233) continue;
    const size_t target = ((y + kScreenY) * kWidth + x + kScreenX) * 3;
    const size_t source = (y * kScreenSize + x) * 3;
    std::copy_n(screen.data() + source, 3, pixels_.data() + target);
  }
  footer_.fillScreen(0x1082);
  footer_.setTextDatum(middle_center);
  footer_.setFont(&fonts::efontCN_24);
  footer_.setTextColor(0xD6BA);
  char status[128];
  std::snprintf(status, sizeof status, "W 走路%s  ·  Tab %s  ·  %llu 步", platform_.walking() ? "中" : "关",
                platform_.fast() ? "16倍" : "1倍", static_cast<unsigned long long>(runtime_->engine().view().totalSteps));
  footer_.drawString(status, kWidth / 2, 20);
  footer_.setFont(&fonts::Font2);
  footer_.drawString("A / Left     D / Right     Enter = A+B", kWidth / 2, 50);
  footer_.drawString("Mouse: touch / swipe    P: power    F12: screenshot", kWidth / 2, 74);
  footer_.setFont(&fonts::efontCN_24);
  footer_.drawString(closing_ ? "正在保存并退出" : runtime_->dimmed() ? "已熄屏 · 按 P 唤醒" : "桌面模拟 · 关闭窗口保存", kWidth / 2, 108);
  footer_.readRectRGB(0, 0, kWidth, kHeight - 504, pixels_.data() + 504 * kWidth * 3);
  require(SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kWidth * 3) == 0);
  require(SDL_RenderClear(renderer_) == 0);
  require(SDL_RenderCopy(renderer_, texture_, nullptr, nullptr) == 0);
  SDL_RenderPresent(renderer_);
}
std::string App::screenshot(const std::string& name) {
  const std::string path = screenshots_ + "/" + name + ".bmp";
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kWidth, kHeight, 24, SDL_PIXELFORMAT_BGR24);
  require(surface != nullptr);
  const bool ok = SDL_RenderReadPixels(renderer_, nullptr, surface->format->format, surface->pixels, surface->pitch) == 0 &&
                  SDL_SaveBMP(surface, path.c_str()) == 0;
  SDL_FreeSurface(surface); require(ok);
  std::printf("截图：%s\n", path.c_str()); return path;
}
void App::reportError(const std::string& message) {
  if (options_.error) options_.error(message);
  else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "保存失败", message.c_str(), window_);
}
bool App::close() {
  using pokedex::adventure::CommandResult;
  auto& engine = runtime_->engine();
  if (engine.savePending()) engine.retrySave();
  if (engine.savePending()) {
    reportError("请修复存档目录权限后再次关闭。");
    return false;
  }
  if (engine.ready()) {
    const auto result = runtime_->checkpoint();
    if (engine.savePending() || (result != CommandResult::Applied && result != CommandResult::Duplicate)) {
      reportError("模拟器仍在运行，请修复存档目录权限后再次关闭。");
      return false;
    }
  }
  if (!platform_.saveClock()) {
    reportError("时钟保存失败，模拟器仍在运行，请检查存档目录权限后重试。");
    return false;
  }
  return true;
}
}
