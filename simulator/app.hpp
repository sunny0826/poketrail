#pragma once
#include "platform.hpp"
#include "fault_io.hpp"
#include <memory>
#include <string>
#include <vector>

namespace simulator {
class App {
 public:
  App(const std::string& saveDir, const std::string& screenshots, const Options& options = Options());
  ~App();
  bool step();
  void draw();
  bool close();
  std::string screenshot(const std::string& name);
  pokedex::ui::Runtime& runtime() { return *runtime_; }
  Platform& platform() { return platform_; }
  void fault(Fault value) { io_.fault(value); }
  unsigned faultHits() const { return io_.hits(); }
  unsigned writes() const { return io_.writes(); }
 private:
  void reportError(const std::string& message);
  Options options_;
  Storage storage_;
  FaultIO io_;
  pokedex::adventure::FileStore store_;
  Platform platform_;
  std::unique_ptr<pokedex::ui::Runtime> runtime_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  lgfx::LGFX_Sprite footer_;
  std::vector<uint8_t> pixels_;
  std::string screenshots_;
  unsigned shots_ = 0;
  bool running_ = true, closing_ = false;
};
}
