#pragma once
#include "adventure_store.hpp"
#include "test_mode.hpp"
#include <cstdint>

namespace pokedex { namespace adventure {

enum class TestScenario : uint8_t {
  AccountBalls,
  NearEncounter,
  CurrentThrow,
  LegacyEncounter
};

class TestConsole {
 public:
  static bool seed(Store&, TestModeHost&, TestScenario);
};

} }
