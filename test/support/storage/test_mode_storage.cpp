#define STORAGE_TEST_IMPLEMENTATION
#include "LittleFS.h"
#include "sys/stat.h"
#include "esp_littlefs.h"
#include "../../../firmware/apps/20_pokedex/device_store.hpp"
#include "../../../firmware/apps/20_pokedex/save_codec.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace pokedex::adventure;
using storage_test::state;
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "test_mode line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static storage_test::Node* node(const char* path) {
  const auto i = state.nodes.find(std::string("/littlefs") + path);
  return i == state.nodes.end() ? nullptr : &i->second;
}
static std::vector<uint8_t> raw(const char* path) {
  const storage_test::Node* n = node(path);
  return n ? n->bytes : std::vector<uint8_t>();
}
static uint64_t hashBytes(const std::vector<uint8_t>& b) {
  uint64_t h = 1469598103934665603ull;
  for (uint8_t v : b) { h ^= v; h *= 1099511628211ull; }
  return h;
}
static void putRaw(const char* path, const std::vector<uint8_t>& bytes) {
  state.nodes[std::string("/littlefs") + path].bytes = bytes;
}
static void officialDirectory() {
  state.nodes["/littlefs/adventure"].directory = true;
}

static void officialBytesStay() {
  state = storage_test::State();
  officialDirectory();
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::Loaded);
  Save a; a.sequence = 1; a.encounterTarget = 300;
  CHECK(d.commit(a) == CommitStatus::Committed);
  Save b = a; b.sequence = 2; b.encounterTarget = 333;
  CHECK(d.commit(b) == CommitStatus::Committed);
  const auto beforeA = raw("/adventure/save-a.bin");
  const auto beforeB = raw("/adventure/save-b.bin");
  const uint64_t hashA = hashBytes(beforeA), hashB = hashBytes(beforeB);
  CHECK(!beforeA.empty() && !beforeB.empty());
  const size_t officialBytes = d.diagnostics().officialBytes;
  CHECK(officialBytes == beforeA.size() + beforeB.size());

  CHECK(d.enterTest(TestSeed::Snapshot));
  CHECK(d.testMode() && !d.ramOnly());
  CHECK(raw("/adventure-test/save-a.bin") == beforeA);
  CHECK(raw("/adventure-test/save-b.bin") == beforeB);
  Save loaded;
  CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 2);
  Save test = loaded; test.sequence = 3; test.encounterTarget = 333;
  CHECK(d.commit(test) == CommitStatus::Committed);
  CHECK(raw("/adventure/save-a.bin") == beforeA);
  CHECK(raw("/adventure/save-b.bin") == beforeB);

  CHECK(d.leaveTest());
  CHECK(!d.testMode());
  CHECK(raw("/adventure/save-a.bin") == beforeA);
  CHECK(raw("/adventure/save-b.bin") == beforeB);
  CHECK(hashBytes(raw("/adventure/save-a.bin")) == hashA);
  CHECK(hashBytes(raw("/adventure/save-b.bin")) == hashB);
  CHECK(node("/adventure-test/active") == nullptr);
  CHECK(node("/adventure-test") == nullptr);
  Save after;
  CHECK(d.load(after).status == LoadStatus::Loaded && after.sequence == 2);
  CHECK(state.formats == 0);
}

static void blankSeed() {
  state = storage_test::State();
  officialDirectory();
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::Loaded);
  Save a; a.sequence = 1; a.encounterTarget = 300;
  CHECK(d.commit(a) == CommitStatus::Committed);
  const auto before = raw("/adventure/save-a.bin");
  CHECK(d.enterTest(TestSeed::Blank));
  CHECK(d.testMode());
  CHECK(node("/adventure-test/save-a.bin") == nullptr);
  CHECK(node("/adventure-test/save-b.bin") == nullptr);
  Save empty;
  CHECK(d.load(empty).status == LoadStatus::Empty);
  Save fresh; fresh.sequence = 1; fresh.encounterTarget = 320;
  CHECK(d.commit(fresh) == CommitStatus::Committed);
  CHECK(!raw("/adventure-test/save-a.bin").empty());
  CHECK(raw("/adventure/save-a.bin") == before);
  CHECK(d.leaveTest());
  CHECK(raw("/adventure/save-a.bin") == before);
  CHECK(state.formats == 0);
}

static void markerSurvivesRestart() {
  state = storage_test::State();
  officialDirectory();
  {
    DeviceStore d;
    CHECK(d.begin() == LoadStatus::Loaded);
    Save a; a.sequence = 1; a.encounterTarget = 300;
    CHECK(d.commit(a) == CommitStatus::Committed);
    Save b = a; b.sequence = 2;
    CHECK(d.commit(b) == CommitStatus::Committed);
    CHECK(d.enterTest(TestSeed::Snapshot));
    Save test = b; test.sequence = 3;
    CHECK(d.commit(test) == CommitStatus::Committed);
  }
  CHECK(node("/adventure-test/active") != nullptr);
  {
    DeviceStore reboot;
    CHECK(reboot.begin() == LoadStatus::Loaded);
    CHECK(reboot.testMode());
    Save loaded;
    CHECK(reboot.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 3);
    CHECK(reboot.leaveTest());
  }
  {
    DeviceStore reboot;
    CHECK(reboot.begin() == LoadStatus::Loaded);
    CHECK(!reboot.testMode());
    Save loaded;
    CHECK(reboot.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 2);
  }
  CHECK(state.formats == 0);
}

static void markerReadFailureIsIoError() {
  state = storage_test::State();
  officialDirectory();
  putRaw("/adventure-test/active", {});
  state.statError = true;
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::IoError);
  CHECK(!d.testMode());
  state.statError = false;
  CHECK(d.begin() == LoadStatus::Loaded && d.testMode());
  CHECK(d.leaveTest());
  CHECK(state.formats == 0);
}

static void cleanupFailureKeepsOfficial() {
  state = storage_test::State();
  officialDirectory();
  Save loaded;
  {
    DeviceStore d;
    CHECK(d.begin() == LoadStatus::Loaded);
    Save a; a.sequence = 1; a.encounterTarget = 300;
    CHECK(d.commit(a) == CommitStatus::Committed);
    CHECK(d.enterTest(TestSeed::Blank));
    Save test; test.sequence = 1; test.encounterTarget = 310;
    CHECK(d.commit(test) == CommitStatus::Committed);
    state.failRemove = "/adventure-test/save-a.bin";
    CHECK(d.leaveTest());
    CHECK(!d.testMode());
    CHECK(d.diagnostics().cleanupFailed);
    CHECK(node("/adventure-test/active") == nullptr);
    CHECK(node("/adventure-test/save-a.bin") != nullptr);
    CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 1);
  }
  state.failRemove.clear();
  {
    DeviceStore reboot;
    CHECK(reboot.begin() == LoadStatus::Loaded);
    CHECK(!reboot.testMode());
    CHECK(reboot.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 1);
  }
  CHECK(state.formats == 0);
}

static void corruptOfficialStillEnters() {
  state = storage_test::State();
  officialDirectory();
  putRaw("/adventure/save-a.bin", {'n', 'o', 't', 'a', 's', 'a', 'v', 'e'});
  const auto bad = raw("/adventure/save-a.bin");
  {
    DeviceStore d;
    CHECK(d.begin() == LoadStatus::Loaded);
    CHECK(d.enterTest(TestSeed::Snapshot));
    CHECK(raw("/adventure/save-a.bin") == bad);
    CHECK(raw("/adventure-test/save-a.bin") == bad);
    Save out;
    CHECK(d.load(out).status == LoadStatus::Corrupt);
    CHECK(d.leaveTest());
    CHECK(raw("/adventure/save-a.bin") == bad);
  }

  state = storage_test::State();
  officialDirectory();
  std::vector<uint8_t> future(codec::kHeaderSize, 0);
  future[0] = 'A'; future[1] = 'V'; future[2] = 'D'; future[3] = '1';
  future[4] = 99; future[5] = 0;
  putRaw("/adventure/save-a.bin", future);
  {
    DeviceStore d;
    CHECK(d.begin() == LoadStatus::Loaded);
    CHECK(d.enterTest(TestSeed::Snapshot));
    Save out;
    CHECK(d.load(out).status == LoadStatus::UnsupportedVersion);
    CHECK(d.leaveTest());
    CHECK(raw("/adventure/save-a.bin") == future);
  }
  CHECK(state.formats == 0);
}

static void ramOnlySession() {
  state = storage_test::State();
  state.mount = false;
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::NeedsProvisioning);
  CHECK(!d.enterTest(TestSeed::Snapshot));
  CHECK(!d.testMode());
  CHECK(d.enterTest(TestSeed::Blank));
  CHECK(d.testMode() && d.ramOnly());
  Save empty;
  CHECK(d.load(empty).status == LoadStatus::Empty);
  Save a; a.sequence = 1; a.encounterTarget = 300;
  CHECK(d.commit(a) == CommitStatus::Committed);
  Save loaded;
  CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 1);
  CHECK(state.writes == 0);
  d.setFault(TestFault::Load);
  CHECK(d.load(loaded).status == LoadStatus::IoError);
  d.setFault(TestFault::None);
  CHECK(d.load(loaded).status == LoadStatus::Loaded);
  CHECK(d.leaveTest());
  CHECK(!d.testMode() && !d.ramOnly());
  CHECK(d.load(loaded).status == LoadStatus::NeedsProvisioning);
  CHECK(state.formats == 0);
}

static void faultInjection() {
  state = storage_test::State();
  officialDirectory();
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::Loaded);
  Save a; a.sequence = 1; a.encounterTarget = 300;
  CHECK(d.commit(a) == CommitStatus::Committed);
  CHECK(d.enterTest(TestSeed::Blank));

  d.setFault(TestFault::CommitBefore);
  CHECK(d.commit(a) == CommitStatus::IoError);
  CHECK(node("/adventure-test/save-a.bin") == nullptr && node("/adventure-test/save-b.bin") == nullptr);

  d.setFault(TestFault::CommitAfter);
  CHECK(d.commit(a) == CommitStatus::IoError);
  CHECK(node("/adventure-test/save-a.bin") != nullptr);
  Save loaded;
  d.setFault(TestFault::None);
  CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 1);
  const int writes = state.writes;
  CHECK(d.commit(a) == CommitStatus::Committed && state.writes == writes);

  d.setFault(TestFault::Load);
  CHECK(d.load(loaded).status == LoadStatus::IoError);
  CHECK(d.leaveTest());
  // Official mode ignores a stale fault switch.
  d.setFault(TestFault::Load);
  CHECK(!d.testMode());
  CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 1);
  d.setFault(TestFault::None);
  CHECK(state.formats == 0);
}

static void repeatedBeginKeepsState() {
  state = storage_test::State();
  officialDirectory();
  {
    DeviceStore d;
    CHECK(d.begin() == LoadStatus::Loaded);
    CHECK(d.enterTest(TestSeed::Blank));
    d.setFault(TestFault::CommitAfter);
    CHECK(d.begin() == LoadStatus::Loaded);
    CHECK(d.testMode() && d.fault() == TestFault::CommitAfter);
    Save a; a.sequence = 1; a.encounterTarget = 300;
    CHECK(d.commit(a) == CommitStatus::IoError);
    d.setFault(TestFault::None);
    CHECK(d.commit(a) == CommitStatus::Committed);
    CHECK(d.begin() == LoadStatus::Loaded);
    CHECK(d.testMode() && !d.ramOnly());
    Save loaded;
    CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 1);
    CHECK(d.leaveTest());
  }

  state = storage_test::State();
  state.mount = false;
  DeviceStore ram;
  CHECK(ram.begin() == LoadStatus::NeedsProvisioning);
  CHECK(ram.enterTest(TestSeed::Blank));
  Save a; a.sequence = 1; a.encounterTarget = 300;
  CHECK(ram.commit(a) == CommitStatus::Committed);
  CHECK(ram.begin() == LoadStatus::Loaded);
  CHECK(ram.testMode() && ram.ramOnly());
  Save loaded;
  CHECK(ram.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 1);
  Save b = a; b.sequence = 2; b.encounterTarget = 310;
  CHECK(ram.commit(b) == CommitStatus::Committed);
  CHECK(ram.load(loaded).status == LoadStatus::Loaded && loaded.sequence == 2);
  CHECK(state.writes == 0);
  CHECK(ram.leaveTest());
  CHECK(state.formats == 0);
}

static void activeDeleteFailureKeepsTest() {
  state = storage_test::State();
  officialDirectory();
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::Loaded);
  Save official; official.sequence = 1; official.encounterTarget = 300;
  CHECK(d.commit(official) == CommitStatus::Committed);
  CHECK(d.enterTest(TestSeed::Blank));
  Save test; test.sequence = 1; test.encounterTarget = 310;
  CHECK(d.commit(test) == CommitStatus::Committed);
  state.failRemove = "/adventure-test/active";
  CHECK(!d.leaveTest());
  CHECK(d.testMode());
  Save loaded;
  CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.encounterTarget == 310);
  state.failRemove.clear();
  CHECK(d.leaveTest());
  CHECK(!d.testMode());
  CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.encounterTarget == 300);
  CHECK(state.formats == 0);
}

static void pendingDoesNotCrossRoots() {
  state = storage_test::State();
  officialDirectory();
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::Loaded);
  Save a; a.sequence = 1; a.encounterTarget = 300;
  state.shortWrite = true;
  CHECK(d.commit(a) == CommitStatus::IoError);
  state.shortWrite = false;
  CHECK(d.enterTest(TestSeed::Blank));
  Save test; test.sequence = 1; test.encounterTarget = 310;
  CHECK(d.commit(test) == CommitStatus::Committed);
  Save loaded;
  CHECK(d.load(loaded).status == LoadStatus::Loaded && loaded.encounterTarget == 310);
  CHECK(d.leaveTest());
  CHECK(state.formats == 0);
}

int main() {
  officialBytesStay();
  blankSeed();
  markerSurvivesRestart();
  markerReadFailureIsIoError();
  cleanupFailureKeepsOfficial();
  corruptOfficialStillEnters();
  ramOnlySession();
  faultInjection();
  repeatedBeginKeepsState();
  activeDeleteFailureKeepsTest();
  pendingDoesNotCrossRoots();
  std::printf("PASS test_mode_storage: %d checks\n", checks);
}
