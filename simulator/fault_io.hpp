#pragma once
#include "storage.hpp"

namespace simulator {
class FaultIO : public pokedex::adventure::SlotIO {
 public:
  explicit FaultIO(Storage& storage) : storage_(storage) {}
  void fault(Fault value) { fault_ = value; readBack_ = false; storage_.clockFault(value == Fault::ClockWrite); }
  unsigned hits() const { return hits_ + storage_.clockFaultHits(); }
  unsigned writes() const { return writes_; }
  pokedex::adventure::LoadStatus prepare() override { return storage_.prepare(); }
  pokedex::adventure::ReadStatus read(unsigned slot, uint8_t* out, size_t capacity, size_t& length) override {
    if (readBack_ && slot == writtenSlot_) {
      readBack_ = false; ++hits_; return pokedex::adventure::ReadStatus::IoError;
    }
    return storage_.read(slot, out, capacity, length);
  }
  bool write(unsigned slot, const uint8_t* bytes, size_t length) override {
    ++writes_;
    if (fault_ == Fault::BeforeWrite) { ++hits_; return false; }
    if (fault_ == Fault::PartialWrite) {
      ++hits_; storage_.write(slot, bytes, length / 2); return false;
    }
    if (!storage_.write(slot, bytes, length)) return false;
    if (fault_ == Fault::AfterWrite) { ++hits_; return false; }
    if (fault_ == Fault::ReadBack) { writtenSlot_ = slot; readBack_ = true; }
    return true;
  }
 private:
  Storage& storage_;
  Fault fault_ = Fault::None;
  unsigned hits_ = 0, writes_ = 0, writtenSlot_ = 0;
  bool readBack_ = false;
};
}
