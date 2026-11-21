#define STORAGE_TEST_IMPLEMENTATION
#include "LittleFS.h"
#include "sys/stat.h"
#include "esp_littlefs.h"
#include "../../../firmware/apps/20_pokedex/device_store.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace pokedex::adventure;
using storage_test::state;
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"device line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
static void lifecycle() {
  state = storage_test::State();
  Save out; Save a; a.sequence = 1; a.encounterTarget = 300;
  DeviceStore d;
  CHECK(d.load(out).status == LoadStatus::NeedsProvisioning);
  CHECK(!d.capacity().valid);
  state.mount = false;
  CHECK(d.begin() == LoadStatus::NeedsProvisioning && state.correctMount);
  CHECK(d.commit(a) == CommitStatus::IoError && state.writes == 0);
  state.mount = true; state.info = false;
  CHECK(d.begin() == LoadStatus::IoError);
  state.info = true; state.mkdir = false;
  CHECK(d.begin() == LoadStatus::IoError);
  state.mkdir = true;
  state.nodes["/littlefs/adventure"].directory = false;
  CHECK(d.begin() == LoadStatus::IoError);
  state.nodes.clear();
  CHECK(d.begin() == LoadStatus::Loaded);
  CHECK(d.load(out).status == LoadStatus::Empty);
  DeviceStore other;
  CHECK(other.begin() == LoadStatus::IoError);
  state.statError = true;
  CHECK(d.load(out).status == LoadStatus::IoError);
  state.statError = false;
  state.used = state.total + 1;
  CHECK(!d.capacity().valid && d.commit(a) == CommitStatus::IoError);
  state.used = state.total;
  CHECK(d.commit(a) == CommitStatus::IoError && state.writes == 0);
  state.used = 1000; state.open = false;
  CHECK(d.commit(a) == CommitStatus::IoError);
  state.open = true;
  CHECK(d.commit(a) == CommitStatus::Committed);
  state.open = false;
  CHECK(d.load(out).status == LoadStatus::IoError);
  state.open = true; state.read = false;
  CHECK(d.load(out).status == LoadStatus::IoError);
  state.read = true;
  a.sequence = 2; state.shortWrite = true;
  CHECK(d.commit(a) == CommitStatus::IoError);
  CHECK(d.load(out).status == LoadStatus::Loaded && out.sequence == 1);
  state.shortWrite = false; state.loseFlush = true;
  CHECK(d.commit(a) == CommitStatus::IoError);
  CHECK(d.load(out).status == LoadStatus::Loaded && out.sequence == 1);
  state.loseFlush = false;
  CHECK(d.commit(a) == CommitStatus::Committed);
  state.nodes["/littlefs/adventure/save-a.bin"].directory = true;
  CHECK(d.load(out).status == LoadStatus::IoError);
  CHECK(d.commit(a) == CommitStatus::IoError);
  CHECK(state.formats == 0);
}
static void capacityAndDirectoryRecovery() {
  state = storage_test::State();
  Save out;
  Save a; a.sequence = 1; a.encounterTarget = 300;
  Save b = a; b.sequence = 2;
  Save c = a; c.sequence = 3;
  DeviceStore d;
  CHECK(d.begin() == LoadStatus::Loaded);
  CHECK(d.commit(a) == CommitStatus::Committed);

  const int writesBeforeInfoLoss = state.writes;
  state.info = false;
  CHECK(d.load(out).status == LoadStatus::Loaded && out.sequence == 1);
  CHECK(d.commit(b) == CommitStatus::IoError && state.writes == writesBeforeInfoLoss);
  state.info = true;
  CHECK(d.commit(b) == CommitStatus::Committed);
  CHECK(d.load(out).status == LoadStatus::Loaded && out.sequence == 2);

  const int writesBeforeOveruse = state.writes;
  state.used = state.total + 1;
  CHECK(d.commit(c) == CommitStatus::IoError && state.writes == writesBeforeOveruse);
  state.used = 1000;
  CHECK(d.load(out).status == LoadStatus::Loaded && out.sequence == 2);

  state.nodes.erase("/littlefs/adventure");
  state.mkdir = false;
  const int writesBeforeMissingDir = state.writes;
  CHECK(d.commit(c) == CommitStatus::IoError && state.writes == writesBeforeMissingDir);
  CHECK(d.begin() == LoadStatus::IoError);
  state.mkdir = true;
  CHECK(d.commit(c) == CommitStatus::Committed);
  CHECK(d.load(out).status == LoadStatus::Loaded && out.sequence == 3);

  state.open = false;
  Save next = a; next.sequence = 4;
  CHECK(d.commit(next) == CommitStatus::IoError);
  state.open = true;
  state.nodes.erase("/littlefs/adventure");
  state.mkdir = false;
  CHECK(d.commit(next) == CommitStatus::IoError);
  state.mkdir = true;
  CHECK(d.commit(next) == CommitStatus::Committed);
  CHECK(d.load(out).status == LoadStatus::Loaded && out.sequence == 4);
  CHECK(state.formats == 0);
}
int main() {
  lifecycle();
  {
    DeviceStore reboot; Save out;
    state.nodes["/littlefs/adventure/save-a.bin"].directory = false;
    CHECK(reboot.begin() == LoadStatus::Loaded);
    CHECK(reboot.load(out).status == LoadStatus::Loaded && out.sequence == 2);
    CHECK(state.formats == 0);
  }
  capacityAndDirectoryRecovery();
  std::printf("PASS device_store: %d checks (host LittleFS/VFS substitutes)\n", checks);
}
