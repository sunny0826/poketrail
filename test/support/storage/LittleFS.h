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
  size_t total = 1000000, used = 1000;
  int formats = 0, writes = 0, closes = 0, flushes = 0;
  bool correctMount = false;
};
extern State state;
}
class File {
 public:
  File() : node_(nullptr) {}
  explicit File(storage_test::Node* n) : node_(n) {}
  operator bool() const { return node_ != nullptr; }
  bool isDirectory() { return node_->directory; }
  size_t size() { return node_->bytes.size(); }
  size_t read(uint8_t*, size_t);
  size_t write(const uint8_t*, size_t);
  void flush();
  void close();
 private:
  storage_test::Node* node_;
};
class FakeLittleFS {
 public:
  bool begin(bool, const char*, uint8_t, const char*);
  bool mkdir(const char*);
  File open(const char*, const char*);
  void end() {}
  bool format() { ++storage_test::state.formats; return true; }
};
extern FakeLittleFS LittleFS;
