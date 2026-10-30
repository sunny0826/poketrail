#pragma once
#include "file_store.hpp"
#include "test_mode.hpp"
#include <memory>

namespace pokedex { namespace adventure {
struct StorageCapacity {
  bool valid = false;
  size_t totalBytes = 0;
  size_t usedBytes = 0;
  size_t availableBytes = 0;
};
// One runtime owner. No other code may mount/unmount/use LittleFS concurrently.
// The same mounted filesystem serves both roots; only the directory changes.
class DeviceStore : private SlotIO, public Store, public TestModeHost {
 public:
  DeviceStore();
  ~DeviceStore();
  LoadStatus begin();
  LoadResult load(Save& out) override;
  CommitStatus commit(const Save& candidate) override;
  StorageCapacity capacity() const;

  bool testMode() const override { return mode_.active; }
  bool ramOnly() const override { return mode_.active && mode_.ramOnly; }
  bool enterTest(TestSeed seed) override;
  bool leaveTest() override;
  TestMode& mode() override { return mode_; }
  void setFault(TestFault fault) override { mode_.fault = fault; }
  TestFault fault() const override { return mode_.fault; }
  TestDiagnostics diagnostics() const override;

  DeviceStore(const DeviceStore&) = delete;
  DeviceStore& operator=(const DeviceStore&) = delete;
 private:
  LoadStatus prepare() override;
  ReadStatus read(unsigned slot, uint8_t* out, size_t capacity, size_t& length) override;
  bool write(unsigned slot, const uint8_t* bytes, size_t length) override;
  bool seedSnapshot();
  const char* root_ = "/adventure";
  TestMode mode_;
  LargeObject<Save> ramSave_;
  bool ramHasSave_ = false;
  bool mounted_ = false;
  bool initialized_ = false;
  FileStore store_;
};
} }
