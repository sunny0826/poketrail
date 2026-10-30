#include "support/adventure_ui/harness.hpp"
#include "test_mode_platform.hpp"
#include "test_console.hpp"
#include <memory>

namespace {
using pokedex::adventure::TestDiagnostics;
using pokedex::adventure::TestFault;
using pokedex::adventure::TestMode;
using pokedex::adventure::TestModeHost;
using pokedex::adventure::TestScenario;
using pokedex::adventure::TestSeed;
using pokedex::adventure::TestConsole;
using pokedex::ui::Page;
using pokedex::ui::Runtime;
using pokedex::ui::TestModePlatform;

struct HostStore : MemoryStore {
  TestModeHost* host = nullptr;
  pokedex::adventure::LoadResult load(pokedex::adventure::Save& save) override {
    if (host && host->testMode() && host->fault() == TestFault::Load)
      return pokedex::adventure::LoadResult(pokedex::adventure::LoadStatus::IoError);
    return MemoryStore::load(save);
  }
  pokedex::adventure::CommitStatus commit(const pokedex::adventure::Save& save) override {
    if (host && host->testMode() && host->fault() == TestFault::CommitBefore)
      return pokedex::adventure::CommitStatus::IoError;
    const auto result = MemoryStore::commit(save);
    if (host && host->testMode() && host->fault() == TestFault::CommitAfter)
      return pokedex::adventure::CommitStatus::IoError;
    return result;
  }
};

struct ModeHost : TestModeHost {
  MemoryStore official;
  HostStore test;
  TestMode modeState;
  bool active = false;
  bool ramOnlyMode = false;
  bool failLeave = false;

  ModeHost() { test.host = this; }
  bool testMode() const override { return active; }
  bool ramOnly() const override { return active && ramOnlyMode; }
  bool enterTest(TestSeed seed) override {
    if (seed == TestSeed::Snapshot) {
      test.bytes = official.bytes;
      test.status = official.status;
    } else {
      test.bytes.clear();
      test.status = pokedex::adventure::LoadStatus::Empty;
    }
    modeState = TestMode();
    modeState.active = true;
    active = true;
    ramOnlyMode = false;
    return true;
  }
  bool leaveTest() override {
    if (failLeave) return false;
    active = false;
    ramOnlyMode = false;
    modeState = TestMode();
    test.bytes.clear();
    test.status = pokedex::adventure::LoadStatus::Empty;
    return true;
  }
  TestMode& mode() override { return modeState; }
  void setFault(TestFault fault) override { modeState.fault = fault; }
  TestFault fault() const override { return modeState.fault; }
  TestDiagnostics diagnostics() const override {
    TestDiagnostics d;
    d.mounted = true;
    d.testMode = active;
    d.ramOnly = ramOnly();
    d.officialBytes = official.bytes.size();
    d.testBytes = active ? test.bytes.size() : 0;
    d.capacityBytes = 8u * 1024u * 1024u;
    d.availableBytes = 6u * 1024u * 1024u;
    return d;
  }
  MemoryStore& store() { return active ? test : official; }
};

struct Fixture {
  ModeHost host;
  TestPlatform real;
  TestModePlatform platform;
  std::unique_ptr<Runtime> runtime;
  std::vector<uint8_t> officialBefore;
  Fixture() : platform(real, host) {}
  void boot(bool persistBoot = true) {
    runtime.reset();
    runtime.reset(new Runtime(host.store(), platform, &host));
    check(runtime->begin(persistBoot), "test-mode runtime graphics allocated");
    frame(*runtime);
  }
  Runtime& r() { return *runtime; }
  TestPlatform& p() { return real; }
};

#include "support/adventure_ui/test_mode_scenarios.hpp"

bool testBadgeVisible() {
  for (int y = 444; y <= 460; ++y)
    for (int x = 210; x <= 256; ++x)
      if (M5.Display.readPixel(x, y) == pokedex::ui::theme::kGold) return true;
  return false;
}

void badge(const char* page) {
  check(testBadgeVisible(), page);
}

void holdTop(Fixture& f, uint32_t duration) {
  for (uint32_t t = 0; t < duration; t += 20) {
    f.p().next.touch = true;
    f.p().next.held = true;
    f.p().next.x = 233;
    f.p().next.y = 40;
    frame(f.r());
  }
  f.p().next.click = true;
  f.p().next.x = 233;
  f.p().next.y = 40;
  frame(f.r());
}

void moveTo(Fixture& f, unsigned item) {
  const auto page = f.r().screen().page;
  for (unsigned guard = 0; f.r().screen().selected != item && guard < 320; ++guard) {
    if (f.r().screen().selected / menu_layout::kPageSize != item / menu_layout::kPageSize) {
      f.p().next.swipe = 1;
      frame(f.r());
    } else edge(f.r(), f.p(), false, true);
  }
  check(f.r().screen().page == page && f.r().screen().selected == item,
        "test-menu item reachable with physical keys");
}

void backWithTouch(Fixture& f) {
  idle(f.r(), 40);
  tap(f.r(), f.p(), 233, 30);
}

void enterFromEntry(Fixture& f, TestSeed seed) {
  const auto request = f.r().takeTestRequest();
  if (seed == TestSeed::Snapshot) check(request == Runtime::TestRequest::EnterSnapshot,
                                        "entry page requests a snapshot copy");
  else check(request == Runtime::TestRequest::EnterBlank, "entry page requests a blank save");
  check(f.r().canSwitchMode(), "entry waits for a safe checkpoint");
  const auto before = f.r().checkpoint();
  check(before == pokedex::adventure::CommandResult::Applied ||
        before == pokedex::adventure::CommandResult::Duplicate,
        "entry checkpoint completes before switching");
  f.officialBefore = f.host.official.bytes;
  check(f.host.enterTest(seed), "test host accepts selected seed");
  if (seed == TestSeed::Snapshot)
    check(f.host.test.bytes == f.officialBefore, "snapshot copies official bytes without migration");
  f.boot();
  f.r().openTestPage(Page::TestHub);
  published(f.r(), seed == TestSeed::Snapshot ? "entered-snapshot" : "entered-blank");
}


void testModeFlow() {
  Fixture f;
  f.boot();
  skipDateAndStarter(f.r(), f.p());
  openMenu(f.r(), f.p());
  select(f.r(), f.p(), 4);
  check(f.r().screen().page == Page::Resources, "resources page opens for hidden entry");
  published(f.r(), "resources-hidden-entry");

  holdTop(f, 1000);
  check(f.r().screen().page == Page::Resources, "short top hold keeps the resources page");
  holdTop(f, 2100);
  check(f.r().screen().page == Page::TestEntry, "two-second top hold opens entry confirmation");
  published(f.r(), "test-entry");
  check(!testBadgeVisible(), "entry confirmation remains outside test mode");

  backWithTouch(f);
  check(f.r().screen().page == Page::Resources, "entry confirmation returns to its origin");
  idle(f.r(), 40);
  holdTop(f, 2100);
  check(f.r().screen().page == Page::TestEntry, "entry confirmation opens again");
  idle(f.r(), 40);
  select(f.r(), f.p(), 0);
  enterFromEntry(f, TestSeed::Snapshot);
  check(f.host.active && f.r().screen().testMode, "snapshot selection enters test mode");
  badge("test hub shows the test badge");

  moveTo(f, 7);
  published(f.r(), "test-hub-last-page");
  badge("test hub last page keeps the test badge");

  select(f.r(), f.p(), 0);
  check(f.r().screen().page == Page::TestSteps, "test hub opens step injection");
  published(f.r(), "test-steps");
  badge("step injection shows the test badge");
  moveTo(f, 5);
  published(f.r(), "test-steps-last-page");
  backWithTouch(f);
  check(f.r().screen().page == Page::TestHub, "step injection returns to the test hub");

  select(f.r(), f.p(), 3);
  check(f.r().screen().page == Page::TestFaults, "test hub opens fault switches");
  published(f.r(), "test-faults");
  select(f.r(), f.p(), 1);
  check(f.host.fault() == TestFault::CommitBefore, "fault page selects a write-before failure");
  published(f.r(), "test-faults-selected");
  select(f.r(), f.p(), 0);
  check(f.host.fault() == TestFault::None, "fault page restores the no-fault state");
  backWithTouch(f);

  select(f.r(), f.p(), 4);
  check(f.r().screen().page == Page::TestDiagnostics, "test hub opens diagnostics");
  published(f.r(), "test-diagnostics");
  badge("diagnostics shows the test badge");
  check(f.r().screen().testDiagnostics.testBytes == f.host.test.bytes.size(),
        "diagnostics exposes the test slot byte count");
  backWithTouch(f);

  select(f.r(), f.p(), 5);
  check(f.r().screen().page == Page::TestEntry && f.r().screen().testMode,
        "reset uses a confirmation page in test mode");
  published(f.r(), "test-reset-entry");
  badge("reset confirmation shows the test badge");
  select(f.r(), f.p(), 1);
  check(f.r().takeTestRequest() == Runtime::TestRequest::ResetBlank,
        "reset confirmation requests a blank test save");
  check(f.host.enterTest(TestSeed::Blank), "blank reset reaches the test host");
  f.boot();
  f.r().openTestPage(Page::TestHub);
  check(f.r().engine().view().totalSteps == 0 && f.r().engine().view().starter == 0,
        "blank reset produces a fresh test save");
  published(f.r(), "test-reset-blank");

  select(f.r(), f.p(), 2);
  check(f.r().screen().page == Page::TestScenarios, "test hub opens scenario construction");
  published(f.r(), "test-scenarios");
  badge("scenario construction shows the test badge");
  moveTo(f, 4);
  published(f.r(), "test-scenarios-last-page");
  badge("scenario last page keeps the test badge");
  select(f.r(), f.p(), 1);
  check(f.r().takeTestRequest() == Runtime::TestRequest::ScenarioNearEncounter,
        "scenario page requests a near-encounter state");
  f.runtime.reset();
  check(TestConsole::seed(f.host.test, f.host, TestScenario::NearEncounter),
        "near-encounter scenario encodes, validates and commits");
  f.boot();
  f.r().openTestPage(Page::TestHub);
  check(f.r().engine().view().totalSteps == 299 && f.r().engine().view().encounterTarget == 300,
        "near-encounter scenario loads at 299/300 steps");
  published(f.r(), "test-near-scenario");
  select(f.r(), f.p(), 0);
  select(f.r(), f.p(), 0);
  idle(f.r(), 240);
  check(f.r().engine().view().phase == pokedex::adventure::Phase::EncounterReady,
        "manual +1 step triggers the current-rule encounter");
  check(f.r().engine().view().encounterTarget >= pokedex::adventure::kEncounterMinSteps &&
        f.r().engine().view().encounterTarget <= pokedex::adventure::kEncounterMaxSteps,
        "next encounter target stays within 200-500 steps");
  f.r().openTestPage(Page::Home);
  idle(f.r(), 60);
  check(f.r().screen().page == Page::Home &&
        f.r().engine().view().phase == pokedex::adventure::Phase::EncounterReady,
        "returning to test gameplay shows the triggered encounter");
  openMenu(f.r(), f.p());
  select(f.r(), f.p(), 0);
  idle(f.r(), 60);
  check(f.r().screen().page == Page::Encounter, "triggered encounter opens the encounter page");
  published(f.r(), "test-step-triggered-encounter");
  badge("post-injection page keeps the test badge");
  f.r().openTestPage(Page::TestHub);

  select(f.r(), f.p(), 1);
  check(f.r().screen().page == Page::Clock, "test hub reuses the clock page");
  published(f.r(), "test-clock");
  badge("clock page keeps the test badge");
  const unsigned writes = f.p().clockWrites;
  select(f.r(), f.p(), 4);
  edge(f.r(), f.p(), false, true);
  edge(f.r(), f.p(), true, true);
  select(f.r(), f.p(), 7);
  check(f.p().clockWrites == writes, "simulated clock never writes the real RTC");
  check(f.host.mode().clockStarted, "clock edit updates the simulated clock");
  check(f.r().engine().view().clock.calibrated, "clock edit completes through the Engine");
  published(f.r(), "test-clock-saved");
  badge("home page keeps the test badge after clock edit");

  openMenu(f.r(), f.p());
  select(f.r(), f.p(), 4);
  published(f.r(), "test-resources");
  badge("test-mode resources keeps the test badge");
  idle(f.r(), 40);
  holdTop(f, 2100);
  check(f.r().screen().page == Page::TestHub, "hidden entry reopens the console during test mode");

  select(f.r(), f.p(), 3);
  select(f.r(), f.p(), 3);
  check(f.host.fault() == TestFault::Load, "fault page selects a load failure");
  f.boot();
  check(f.r().screen().page == Page::Error, "load failure reaches the storage error page");
  published(f.r(), "test-load-error");
  badge("test error page keeps the test badge");
  idle(f.r(), 40);
  holdTop(f, 2100);
  check(f.r().screen().page == Page::TestHub, "test error page exposes the test console");
  select(f.r(), f.p(), 3);
  select(f.r(), f.p(), 0);
  check(f.host.fault() == TestFault::None, "load fault is cleared from the console");
  f.boot();
  f.r().openTestPage(Page::TestHub);
  check(f.r().engine().ready(), "rebuild after clearing the load fault recovers the test save");

  pokedex::adventure::Save preEntry;
  check(pokedex::adventure::codec::decode(f.officialBefore.data(), f.officialBefore.size(), preEntry) ==
        pokedex::adventure::codec::DecodeStatus::Ok,
        "pre-entry official save decodes for comparison");
  check(f.host.official.bytes == f.officialBefore,
        "official slots stay byte-identical throughout the test session");

  select(f.r(), f.p(), 6);
  check(f.r().screen().page == Page::TestExit, "test hub opens exit confirmation");
  published(f.r(), "test-exit");
  badge("exit confirmation keeps the test badge");
  select(f.r(), f.p(), 0);
  check(f.r().takeTestRequest() == Runtime::TestRequest::Exit, "exit confirmation requests a mode switch");
  f.host.failLeave = true;
  check(!f.host.leaveTest(), "failing exit keeps the test marker");
  f.boot();
  f.r().openTestPage(Page::TestExit, true);
  check(f.r().screen().page == Page::TestExit && f.r().screen().testMode &&
        f.r().screen().testFailure,
        "failed exit stays on the test exit page with an error");
  published(f.r(), "test-exit-failed");
  badge("failed exit keeps the test badge");
  f.host.failLeave = false;
  select(f.r(), f.p(), 1);
  check(f.r().screen().page == Page::TestHub, "failed exit returns to the test hub");
  select(f.r(), f.p(), 6);
  check(f.r().screen().page == Page::TestExit, "exit confirmation is reachable again");
  select(f.r(), f.p(), 0);
  check(f.r().takeTestRequest() == Runtime::TestRequest::Exit, "retry requests a mode switch");
  check(f.host.leaveTest(), "test host leaves test mode");
  f.boot(false);
  check(!f.r().screen().testMode, "official runtime clears the test marker");
  check(!testBadgeVisible(), "official runtime hides the test badge");
  check(f.r().engine().view().totalSteps == preEntry.totalSteps &&
        f.r().engine().view().starter == preEntry.starter &&
        f.r().engine().view().partner == preEntry.partner,
        "official progress returns unchanged after exit");
}

void viewerBadge() {
  Fixture f;
  f.boot();
  skipDateAndStarter(f.r(), f.p());
  check(f.host.enterTest(TestSeed::Blank), "viewer test host starts a blank session");
  check(TestConsole::seed(f.host.test, f.host, TestScenario::AccountBalls),
        "viewer test save has a starter");
  f.boot();
  f.r().openTestPage(Page::Home);
  openMenu(f.r(), f.p());
  select(f.r(), f.p(), 3);
  idle(f.r(), 80);
  check(f.r().screen().page == Page::Viewer, "pokedex viewer opens in test mode");
  snapCurrent(f.r(), "test-viewer");
  badge("pokedex viewer keeps the test badge");
}

void corruptOfficialEntry() {
  MemoryStore broken;
  broken.status = pokedex::adventure::LoadStatus::Corrupt;
  TestPlatform real;
  ModeHost host;
  TestModePlatform platform(real, host);
  Runtime runtime(broken, platform, &host);
  check(runtime.begin(), "corrupt-official graphics allocated");
  frame(runtime);
  check(runtime.screen().page == Page::Error, "corrupt official save shows the error page");
  published(runtime, "corrupt-official-error");
  for (uint32_t t = 0; t < 2100; t += 20) {
    real.next.touch = real.next.held = true;
    real.next.x = 233;
    real.next.y = 40;
    frame(runtime);
  }
  real.next.click = true;
  real.next.x = 233;
  real.next.y = 40;
  frame(runtime);
  check(runtime.screen().page == Page::TestEntry, "error page exposes the same hidden entry");
  published(runtime, "corrupt-official-entry");
  idle(runtime, 40);
  select(runtime, real, 1);
  check(runtime.takeTestRequest() == Runtime::TestRequest::EnterBlank,
        "error entry can select a blank test save");
}
}

int main() {
  c152::begin("ADVENTURE TEST");
  scenarioSeeds();
  testModeFlow();
  viewerBadge();
  corruptOfficialEntry();
  std::printf("%u test-mode UI checks, 0 failures\n", checks);
  return 0;
}
