#include "support/battle_ui/harness.hpp"
#include "support/battle_ui/runtime_cases.hpp"
#include "support/battle_ui/hp_cases.hpp"

int main() {
  c152::begin("BATTLE TEST");
  attackAndDodge(); perfectAndFinal(); failureAndPractice(); timingAndPause();
  continuousTelegraphRender(); customHpBars(); smallMaxHpBars();
  std::printf("%u battle Runtime/Button_Class/M5GFX checks, 0 failures\n", checks);
}
