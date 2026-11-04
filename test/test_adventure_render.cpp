#include "support/adventure_ui/clock_cases.hpp"

int main() {
  c152::begin("ADVENTURE TEST");
  clockAndErrors(); midnightAndSampling(); slowGaitAttribution(); stallAttribution(); practiceMidnightAchievement();
  std::printf("%u adventure Runtime checks, 0 failures\n", checks);
}
