#pragma once
#include "file_store.hpp"
namespace pokedex { namespace adventure {
struct StorageCapacity {
  bool valid = false;
  size_t totalBytes = 0;
  size_t usedBytes = 0;
  size_t availableBytes = 0;
};
// One runtime owner. No other code may mount/unmount/use LittleFS concurrently.
class DeviceStore : private SlotIO, public Store {
 public:
  DeviceStore();
  ~DeviceStore();
  LoadStatus begin();
  LoadResult load(Save& out) override;
  CommitStatus commit(const Save& candidate) override;
  StorageCapacity capacity() const;
  DeviceStore(const DeviceStore&) = delete;
  DeviceStore& operator=(const DeviceStore&) = delete;
 private:
  LoadStatus prepare() override;
  ReadStatus read(unsigned slot, uint8_t* out, size_t capacity, size_t& length) override;
  bool write(unsigned slot, const uint8_t* bytes, size_t length) override;
  bool mounted_ = false;
  FileStore store_;
};
} }
