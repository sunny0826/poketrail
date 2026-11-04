#include "support/progression_ui.hpp"
#include "support/rendered_text.hpp"
using namespace progression_ui;

int main() {
  c152::begin("startup render");
  Screens unavailable;
  check(!unavailable.startup(0), "missing graphics buffer reports failure without crashing");
  drawGraphicsError({}, i18n::Language::Chinese);
  check(rendered_text::contains("画面不可用，请重启", theme::kRed),
        "graphics failure remains readable without a sprite buffer");
  std::printf("%u startup error checks passed\n", checks);
}
