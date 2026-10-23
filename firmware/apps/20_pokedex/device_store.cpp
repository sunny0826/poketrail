#include "device_store.hpp"
#include <LittleFS.h>
#include <esp_littlefs.h>
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
namespace pokedex { namespace adventure {
namespace {
DeviceStore* owner = nullptr;
const char* const directory = "/littlefs/adventure";
bool fileStat(unsigned slot, struct stat& info, bool& missing) {
  const char* path = FileStore::slotPath(slot);
  if (!path) return false;
  char full[64];
  std::snprintf(full, sizeof(full), "/littlefs%s", path);
  missing = false;
  if (::stat(full, &info) == 0) return true;
  missing = errno == ENOENT;
  return missing;
}
}
DeviceStore::DeviceStore() : store_(*this) {}
DeviceStore::~DeviceStore() {
  if (owner == this) {
    if (mounted_) LittleFS.end();
    owner = nullptr;
  }
}
StorageCapacity DeviceStore::capacity() const {
  StorageCapacity c;
  if (!mounted_ || owner != this) return c;
  if (esp_littlefs_info("spiffs", &c.totalBytes, &c.usedBytes) != ESP_OK ||
      c.totalBytes == 0 || c.usedBytes > c.totalBytes) return StorageCapacity();
  c.availableBytes = c.totalBytes - c.usedBytes;
  c.valid = true;
  return c;
}
LoadStatus DeviceStore::begin() {
  if (owner && owner != this) return LoadStatus::IoError;
  owner = this;
  if (!mounted_) {
    if (!LittleFS.begin(false, "/littlefs", 10, "spiffs")) return LoadStatus::NeedsProvisioning;
    mounted_ = true;
  }
  if (!capacity().valid) return LoadStatus::IoError;
  struct stat info;
  if (::stat(directory, &info) != 0) {
    if (errno != ENOENT || !LittleFS.mkdir("/adventure")) return LoadStatus::IoError;
    if (::stat(directory, &info) != 0) return LoadStatus::IoError;
  }
  return S_ISDIR(info.st_mode) ? LoadStatus::Loaded : LoadStatus::IoError;
}
LoadStatus DeviceStore::prepare() {
  if (owner != this || !mounted_) return LoadStatus::NeedsProvisioning;
  if (!capacity().valid) return LoadStatus::IoError;
  struct stat info;
  return ::stat(directory, &info) == 0 && S_ISDIR(info.st_mode) ?
    LoadStatus::Loaded : LoadStatus::IoError;
}
LoadResult DeviceStore::load(Save& out) { return store_.load(out); }
CommitStatus DeviceStore::commit(const Save& candidate) { return store_.commit(candidate); }
ReadStatus DeviceStore::read(unsigned slot, uint8_t* out, size_t cap, size_t& length) {
  length = 0;
  struct stat info;
  bool missing;
  if (!fileStat(slot, info, missing)) return ReadStatus::IoError;
  if (missing) return ReadStatus::Missing;
  if (!S_ISREG(info.st_mode) || info.st_size < 0) return ReadStatus::IoError;
  File file = LittleFS.open(FileStore::slotPath(slot), "r");
  if (!file) return ReadStatus::IoError;
  length = size_t(info.st_size);
  const size_t n = length < cap ? length : cap;
  const bool ok = !file.isDirectory() && file.size() == length && file.read(out, n) == n &&
                  file.size() == length;
  file.close();
  if (!ok) return ReadStatus::IoError;
  return length > cap ? ReadStatus::TooLarge : ReadStatus::Ok;
}
bool DeviceStore::write(unsigned slot, const uint8_t* bytes, size_t length) {
  if (prepare() != LoadStatus::Loaded) return false;
  struct stat info;
  bool missing;
  if (!fileStat(slot, info, missing) || (!missing && !S_ISREG(info.st_mode))) return false;
  const StorageCapacity c = capacity();
  // Conservative: leave room for a complete copy-on-write candidate, not just its growth.
  if (!c.valid || c.availableBytes < length) return false;
  File file = LittleFS.open(FileStore::slotPath(slot), "w");
  if (!file) return false;
  const bool complete = file.write(bytes, length) == length;
  file.flush(); // Arduino API is void; FileStore proves the result by close/readback.
  file.close();
  return complete;
}
} }
