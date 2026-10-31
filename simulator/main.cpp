#include "app.hpp"
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace simulator { void smoke(App& app, bool reload); }
int main(int argc, char** argv) {
  try {
    if (argc < 3 || argc > 4 || (argc == 4 && std::strcmp(argv[3], "--smoke") &&
        std::strcmp(argv[3], "--reload") && std::strcmp(argv[3], "--corrupt")))
      throw std::runtime_error("请用 mise run simulate 启动，或 mise run test-simulator 验证");
    simulator::App app(argv[1], argv[2]);
    if (argc == 4 && !std::strcmp(argv[3], "--corrupt")) {
      app.step();
      if (app.runtime().engine().ready() || app.runtime().screen().page != pokedex::ui::Page::Error)
        throw std::runtime_error("损坏存档未进入错误页");
      app.screenshot("corrupt-save");
      if (!app.close()) throw std::runtime_error("无法关闭损坏存档页面");
      std::puts("PASS corrupt save opens error page");
    } else if (argc == 4) simulator::smoke(app, !std::strcmp(argv[3], "--reload"));
    else while (true) {
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
