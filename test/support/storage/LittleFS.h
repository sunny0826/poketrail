#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace storage_test {
struct Node { bool directory = false; std::vector<uint8_t> bytes; };
struct State {
  std::map<std::string, Node> nodes;
  bool mount = true, info = true, mkdir = true, open = true, read = true;
  bool statError = false, shortWrite = false, loseFlush = false;
  bool remove = true, rmdir = true;
  // Remove fails only for this logical path when non-empty; other paths behave
  // normally so tests can distinguish marker removal from data cleanup.
  std::string failRemove;
  size_t total = 1000000, used = 1000;
  int formats = 0, writes = 0, closes = 0, flushes = 0, removals = 0;
  bool correctMount = false;
};
extern State state;
}
class File {
 public:
  File() : node_(nullptr), position_(0) {}
  explicit File(storage_test::Node* n) : node_(n), position_(0) {}
  operator bool() const { return node_ != nullptr; }
  bool isDirectory() { return node_->directory; }
  size_t size() { return node_->bytes.size(); }
  size_t read(uint8_t*, size_t);
  size_t write(const uint8_t*, size_t);
  void flush();
  void close();
 private:
  storage_test::Node* node_;
  size_t position_;
};
class FakeLittleFS {
 public:
  bool begin(bool, const char*, uint8_t, const char*);
  bool mkdir(const char*);
  File open(const char*, const char*);
  bool remove(const char*);
  bool rmdir(const char*);
  void end() {}
  bool format() { ++storage_test::state.formats; return true; }
};
extern FakeLittleFS LittleFS;

#ifdef STORAGE_TEST_IMPLEMENTATION
#include <algorithm>
#include <cerrno>
#include <cstring>
#include "sys/stat.h"
#include "esp_littlefs.h"
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
  if (!state.info || std::strcmp(label, "spiffs")) return -1;
  *total = state.total; *used = state.used; return ESP_OK;
}
bool FakeLittleFS::begin(bool format, const char* base, uint8_t max, const char* label) {
  state.correctMount = !format && std::strcmp(base, "/littlefs") == 0 && max == 10 &&
                       std::strcmp(label, "spiffs") == 0;
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
bool FakeLittleFS::remove(const char* path) {
  const std::string full = std::string("/littlefs") + path;
  if (!state.remove || (!state.failRemove.empty() && state.failRemove == path)) return false;
  auto i = state.nodes.find(full);
  if (i == state.nodes.end() || i->second.directory) return false;
  state.nodes.erase(i); ++state.removals; return true;
}
bool FakeLittleFS::rmdir(const char* path) {
  const std::string full = std::string("/littlefs") + path;
  if (!state.rmdir) return false;
  auto i = state.nodes.find(full);
  if (i == state.nodes.end() || !i->second.directory) return false;
  for (const auto& entry : state.nodes)
    if (entry.first.compare(0, full.size() + 1, full + "/") == 0) return false;
  state.nodes.erase(i); ++state.removals; return true;
}
size_t File::read(uint8_t* p, size_t n) {
  if (!state.read) return 0;
  const size_t available = position_ < node_->bytes.size() ? node_->bytes.size() - position_ : 0;
  n = std::min(n, available);
  std::copy(node_->bytes.begin() + position_, node_->bytes.begin() + position_ + n, p);
  position_ += n;
  return n;
}
size_t File::write(const uint8_t* p, size_t n) {
  if (state.shortWrite && n) --n;
  if (position_ > node_->bytes.size()) node_->bytes.resize(position_, 0);
  node_->bytes.insert(node_->bytes.begin() + position_, p, p + n);
  position_ += n;
  return n;
}
void File::flush() { ++state.flushes; if (state.loseFlush) node_->bytes.clear(); }
void File::close() { ++state.closes; node_ = nullptr; }
#endif
