#define STORAGE_TEST_IMPLEMENTATION
#include "LittleFS.h"
#include "device_store.hpp"
#include "test_console.hpp"
#include "save_codec.hpp"
#include <cstdio>
#include <cstdlib>
using namespace pokedex::adventure;
namespace {
int budget = -1;
unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "large state line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
class MemoryStore : public Store {
 public:
  Save saved;
  unsigned writes = 0;
  LoadResult load(Save& out) override { out = saved; return LoadResult(LoadStatus::Loaded); }
  CommitStatus commit(const Save& candidate) override {
    saved = candidate; ++writes; return CommitStatus::Committed;
  }
};
class TestHost : public TestModeHost {
 public:
  bool testMode() const override { return true; }
  bool ramOnly() const override { return true; }
  bool enterTest(TestSeed) override { return false; }
  bool leaveTest() override { return false; }
  TestMode& mode() override { return state; }
  void setFault(TestFault fault) override { state.fault = fault; }
  TestFault fault() const override { return state.fault; }
  TestDiagnostics diagnostics() const override { return TestDiagnostics(); }
 private:
  TestMode state;
};
void ramModeAllocation() {
  storage_test::state = storage_test::State();
  storage_test::state.mount = false;
  DeviceStore store;
  CHECK(store.begin() == LoadStatus::NeedsProvisioning);
  budget = 0;
  CHECK(!store.enterTest(TestSeed::Blank));
  CHECK(!store.testMode() && !store.ramOnly());
  Save unchanged; unchanged.sequence = 73;
  CHECK(store.load(unchanged).status == LoadStatus::NeedsProvisioning && unchanged.sequence == 73);
  CHECK(storage_test::state.nodes.empty() && !storage_test::state.writes && !storage_test::state.formats);
  budget = -1;
  CHECK(store.enterTest(TestSeed::Blank) && store.testMode() && store.ramOnly());
  Save candidate; candidate.sequence = 9; candidate.encounterTarget = 300;
  CHECK(store.commit(candidate) == CommitStatus::Committed);
  CHECK(store.load(unchanged).status == LoadStatus::Loaded && unchanged.sequence == 9);
  CHECK(store.leaveTest() && !store.testMode());
  CHECK(storage_test::state.nodes.empty() && !storage_test::state.writes && !storage_test::state.formats);
}
void scenarioAllocation() {
  MemoryStore store; TestHost host;
  store.saved.sequence = 10; store.saved.encounterTarget = 300;
  // Candidate, encoded buffer and the real codec temporary each fail in turn.
  for (int allocation = 0; allocation != 3; ++allocation) {
    budget = allocation;
    CHECK(!TestConsole::seed(store, host, TestScenario::NearEncounter));
    CHECK(store.writes == 0 && store.saved.sequence == 10 && store.saved.totalSteps == 0);
  }
  budget = -1;
  CHECK(TestConsole::seed(store, host, TestScenario::NearEncounter));
  CHECK(store.writes == 1 && store.saved.sequence == 11 && store.saved.totalSteps == 299);
  CHECK(store.saved.partner == 1 && store.saved.progress[0].walked == 299 && validate(store.saved));
}
}
void* heap_caps_malloc(size_t size, uint32_t capabilities) {
  CHECK(capabilities == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!budget) return nullptr;
  if (budget > 0) --budget;
  return std::malloc(size);
}
void heap_caps_free(void* memory) { std::free(memory); }
int main() {
  ramModeAllocation(); scenarioAllocation();
  std::printf("PASS large state: %u checks (PSRAM failure preserves mode and saved scenario)\n", checks);
}
