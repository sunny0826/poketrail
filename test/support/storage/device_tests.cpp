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
namespace storage_test { State state; }
FakeLittleFS LittleFS;
using storage_test::state;
int stat(const char* path, struct stat* out) {
  if (state.statError) { errno = EIO; return -1; }
  const auto i = state.nodes.find(path);
  if (i == state.nodes.end()) { errno = ENOENT; return -1; }
  out->st_mode = i->second.directory ? 1 : 2;
  out->st_size = long(i->second.bytes.size()); return 0;
}
int esp_littlefs_info(const char* label, size_t* total, size_t* used) {
  if (!state.info || std::strcmp(label,"spiffs")) return -1;
  *total = state.total; *used = state.used; return ESP_OK;
}
bool FakeLittleFS::begin(bool format, const char* base, uint8_t max, const char* label) {
  state.correctMount = !format && std::strcmp(base,"/littlefs") == 0 && max == 10 && std::strcmp(label,"spiffs") == 0;
  if (format) ++state.formats;
  return state.mount;
}
bool FakeLittleFS::mkdir(const char* path) {
  if (!state.mkdir) return false;
  state.nodes[std::string("/littlefs") + path].directory = true; return true;
}
File FakeLittleFS::open(const char* path, const char* mode) {
  if (!state.open) return File();
  const std::string full = std::string("/littlefs") + path;
  if (*mode == 'w') { state.nodes[full].bytes.clear(); ++state.writes; }
  auto i = state.nodes.find(full);
  return i == state.nodes.end() ? File() : File(&i->second);
}
size_t File::read(uint8_t* p, size_t n) {
  if (!state.read) return 0;
  n = std::min(n, node_->bytes.size());
  std::copy(node_->bytes.begin(), node_->bytes.begin()+n, p); return n;
}
size_t File::write(const uint8_t* p, size_t n) {
  if (state.shortWrite && n) --n;
  node_->bytes.assign(p,p+n); return n;
}
void File::flush() { ++state.flushes; if (state.loseFlush) node_->bytes.clear(); }
void File::close() { ++state.closes; node_ = nullptr; }
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
  CHECK(d.capacity().valid && d.capacity().availableBytes == 999000);
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
  CHECK(state.flushes == 1 && state.closes >= 2);
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
int main() {
  lifecycle();
  DeviceStore reboot; Save out;
  state.nodes["/littlefs/adventure/save-a.bin"].directory = false;
  CHECK(reboot.begin() == LoadStatus::Loaded);
  CHECK(reboot.load(out).status == LoadStatus::Loaded && out.sequence == 2);
  CHECK(state.formats == 0);
  std::printf("PASS device_store: %d checks (host LittleFS/VFS substitutes)\n", checks);
}
