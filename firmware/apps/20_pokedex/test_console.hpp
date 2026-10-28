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

// Device test control. Scenarios are encoded, decoded and validated before
// their single commit, and the commit is refused outside an active test host.
class TestConsole {
 public:
  static bool seed(Store&, TestModeHost&, TestScenario);
};

const char* testScenarioLabel(TestScenario);
const char* testFaultLabel(TestFault);

} }
