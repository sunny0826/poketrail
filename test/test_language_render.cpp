#include "support/progression_ui.hpp"
#include "support/language_catalogue.hpp"

int main() {
  c152::begin("LANGUAGE TEST");
  language_catalogue::verify();
  std::printf("%u language catalogue checks, 0 failures\n", progression_ui::checks);
}
