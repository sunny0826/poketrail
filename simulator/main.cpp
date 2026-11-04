#include "app.hpp"
#include <cstdio>
#include <stdexcept>

int main(int argc, char** argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("请用 mise run simulate 启动");
    simulator::App app(argv[1], argv[2]);
    while (true) {
      const uint32_t start = SDL_GetTicks();
      if (!app.step()) break;
      const uint32_t elapsed = SDL_GetTicks() - start;
      if (elapsed < 20) SDL_Delay(20 - elapsed);
    }
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "模拟器错误：%s\n", error.what());
    return 1;
  }
}
