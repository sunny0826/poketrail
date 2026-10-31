#pragma once
#include "file_store.hpp"
#include "options.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace simulator {
class Storage : public pokedex::adventure::SlotIO {
 public:
  explicit Storage(const std::string& root, const Options& options = Options())
      : root_(root), wallClock_(options.wallClock) {
    lock_ = ::open((root_ + "/session.lock").c_str(), O_CREAT | O_RDWR, 0600);
    if (lock_ < 0 || flock(lock_, LOCK_EX | LOCK_NB) != 0) {
      if (lock_ >= 0) ::close(lock_);
      throw std::runtime_error("无法锁定模拟存档目录（可能已有模拟器在使用）：" + root_);
    }
  }
  ~Storage() { ::close(lock_); }
  Storage(const Storage&) = delete;
  Storage& operator=(const Storage&) = delete;
  pokedex::adventure::LoadStatus prepare() override {
    struct stat info;
    return stat(root_.c_str(), &info) == 0 && S_ISDIR(info.st_mode)
      ? pokedex::adventure::LoadStatus::Loaded : pokedex::adventure::LoadStatus::IoError;
  }
  pokedex::adventure::ReadStatus read(unsigned slot, uint8_t* out, size_t capacity, size_t& length) override {
    using pokedex::adventure::ReadStatus;
    FILE* file = std::fopen(path(slot).c_str(), "rb");
    if (!file) return errno == ENOENT ? ReadStatus::Missing : ReadStatus::IoError;
    struct stat info;
    if (fstat(fileno(file), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
      std::fclose(file); return ReadStatus::IoError;
    }
    length = size_t(info.st_size);
    const size_t expected = std::min(length, capacity);
    const bool ok = std::fread(out, 1, expected, file) == expected && !std::ferror(file);
    const bool closed = std::fclose(file) == 0;
    if (!ok || !closed) return ReadStatus::IoError;
    return length > capacity ? ReadStatus::TooLarge : ReadStatus::Ok;
  }
  bool write(unsigned slot, const uint8_t* bytes, size_t length) override {
    FILE* file = std::fopen(path(slot).c_str(), "wb");
    if (!file) return false;
    const bool ok = std::fwrite(bytes, 1, length, file) == length &&
                    std::fflush(file) == 0 && fsync(fileno(file)) == 0;
    const bool closed = std::fclose(file) == 0;
    return ok && closed;
  }
  int64_t clockOffset() const {
    FILE* file = std::fopen((root_ + "/clock.txt").c_str(), "r");
    if (!file) {
      if (errno == ENOENT) return 0;
      throw std::runtime_error("无法读取模拟时钟");
    }
    long long offset = 0;
    char extra;
    const bool valid = std::fscanf(file, "%lld %c", &offset, &extra) == 1 && !std::ferror(file);
    std::fclose(file);
    if (!valid || offset < -4102444800LL || offset > 4102444800LL)
      throw std::runtime_error("模拟时钟文件损坏：" + root_ + "/clock.txt");
    return offset;
  }
  void clockFault(bool active) { clockFault_ = active; }
  unsigned clockFaultHits() const { return clockFaultHits_; }
  int64_t wallClock() const { return wallClock_(); }
  bool saveClock(int64_t utc) const {
    if (clockFault_) { ++clockFaultHits_; return false; }
    const std::string target = root_ + "/clock.txt", temporary = target + ".tmp";
    FILE* file = std::fopen(temporary.c_str(), "w");
    if (!file) return false;
    const bool ok = std::fprintf(file, "%lld\n", static_cast<long long>(utc - wallClock())) > 0 &&
                    std::fflush(file) == 0 && fsync(fileno(file)) == 0;
    const bool closed = std::fclose(file) == 0;
    return ok && closed && std::rename(temporary.c_str(), target.c_str()) == 0;
  }
 private:
  std::string path(unsigned slot) const { return root_ + (slot == 0 ? "/save-a.bin" : "/save-b.bin"); }
  std::string root_;
  std::function<int64_t()> wallClock_;
  bool clockFault_ = false;
  mutable unsigned clockFaultHits_ = 0;
  int lock_ = -1;
};
}
