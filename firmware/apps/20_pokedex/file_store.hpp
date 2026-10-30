#pragma once
#include "adventure_store.hpp"
#include "large_object.hpp"
namespace pokedex { namespace adventure {
enum class ReadStatus : uint8_t { Ok, Missing, TooLarge, IoError };
class SlotIO {
 public:
  virtual ~SlotIO() {}
  virtual LoadStatus prepare() = 0; // Loaded means mounted and directory usable.
  // TooLarge supplies the first capacity bytes; length is the full file size.
  virtual ReadStatus read(unsigned slot, uint8_t* out, size_t capacity, size_t& length) = 0;
  // Close on every path. False may still mean the entire candidate reached disk.
  virtual bool write(unsigned slot, const uint8_t* bytes, size_t length) = 0;
};
class FileStore : public Store {
 public:
  explicit FileStore(SlotIO& io);
  ~FileStore();
  LoadResult load(Save& out) override;
  CommitStatus commit(const Save& candidate) override;
  // Drop workspace and any pending retry candidate. Used when the caller
  // retargets the store to another root.
  void reset();
  static const char* slotPath(unsigned slot);
  static size_t workspaceBytes();
  FileStore(const FileStore&) = delete;
  FileStore& operator=(const FileStore&) = delete;
 private:
  struct Workspace;
  SlotIO& io_;
  LargeObject<Workspace> work_;
  bool allocate();
  LoadResult scan(int& current);
};
} }
