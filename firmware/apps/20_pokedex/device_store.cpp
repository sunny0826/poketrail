#include "device_store.hpp"
#include <LittleFS.h>
#include <esp_littlefs.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>
#include <sys/stat.h>

namespace pokedex { namespace adventure {
namespace {
DeviceStore* owner = nullptr;
constexpr const char* kPrefix = "/littlefs";
constexpr const char* kOfficialRoot = "/adventure";
constexpr const char* kTestRoot = "/adventure-test";
constexpr const char* kActivePath = "/adventure-test/active";
constexpr size_t kPathSize = 64;
constexpr size_t kCopyChunk = 512;

const char* slotName(unsigned slot) {
  return slot == 0 ? "save-a.bin" : slot == 1 ? "save-b.bin" : nullptr;
}
bool buildPath(const char* root, unsigned slot, char* out, size_t capacity) {
  const char* name = slotName(slot);
  if (!name) return false;
  const int n = std::snprintf(out, capacity, "%s/%s", root, name);
  return n > 0 && size_t(n) < capacity;
}
bool buildDir(const char* root, char* out, size_t capacity) {
  const int n = std::snprintf(out, capacity, "%s", root);
  return n > 0 && size_t(n) < capacity;
}
// stat takes the mounted prefix, while LittleFS.open takes the logical path.
bool statPath(const char* path, struct stat& info, bool& missing) {
  char full[kPathSize];
  const int n = std::snprintf(full, sizeof(full), "%s%s", kPrefix, path);
  if (n <= 0 || size_t(n) >= sizeof(full)) return false;
  missing = false;
  if (::stat(full, &info) == 0) return true;
  missing = errno == ENOENT;
  return missing;
}
bool ensureDirectory(const char* root) {
  char path[kPathSize];
  if (!buildDir(root, path, sizeof(path))) return false;
  struct stat info;
  bool missing = false;
  if (!statPath(path, info, missing)) return false;
  if (!missing) return S_ISDIR(info.st_mode);
  if (!LittleFS.mkdir(root)) return false;
  if (!statPath(path, info, missing) || missing) return false;
  return S_ISDIR(info.st_mode);
}
enum class MarkerState : uint8_t { Missing, Present, Error };
MarkerState markerState() {
  struct stat info;
  bool missing = false;
  if (!statPath(kActivePath, info, missing)) return MarkerState::Error;
  if (missing) return MarkerState::Missing;
  return S_ISREG(info.st_mode) ? MarkerState::Present : MarkerState::Error;
}
bool writeActive() {
  if (!ensureDirectory(kTestRoot)) return false;
  File file = LittleFS.open(kActivePath, "w");
  if (!file) return false;
  file.close();
  struct stat info;
  bool missing = false;
  return statPath(kActivePath, info, missing) && !missing && S_ISREG(info.st_mode);
}
bool removeFile(const char* path) {
  struct stat info;
  bool missing = false;
  if (!statPath(path, info, missing)) return false;
  if (missing) return true;
  if (S_ISDIR(info.st_mode) || !LittleFS.remove(path)) return false;
  return statPath(path, info, missing) && missing;
}
bool removeDirectory(const char* root) {
  struct stat info;
  bool missing = false;
  if (!statPath(root, info, missing)) return false;
  if (missing) return true;
  if (!S_ISDIR(info.st_mode) || !LittleFS.rmdir(root)) return false;
  return statPath(root, info, missing) && missing;
}
bool clearSlots(const char* root) {
  bool ok = true;
  for (unsigned slot = 0; slot < 2; ++slot) {
    char path[kPathSize];
    if (!buildPath(root, slot, path, sizeof(path)) || !removeFile(path)) ok = false;
  }
  return ok;
}
// Raw byte copy: the test engine, not the snapshot, applies migration.
bool copyFile(const char* source, const char* target) {
  struct stat info;
  bool missing = false;
  if (!statPath(source, info, missing)) return false;
  if (missing) return removeFile(target);
  if (!S_ISREG(info.st_mode) || info.st_size < 0) return false;
  const size_t expected = size_t(info.st_size);
  File in = LittleFS.open(source, "r");
  if (!in) return false;
  File out = LittleFS.open(target, "w");
  if (!out) { in.close(); return false; }
  uint8_t chunk[kCopyChunk];
  bool ok = true;
  size_t copied = 0;
  while (copied < expected) {
    const size_t want = expected - copied < sizeof(chunk) ? expected - copied : sizeof(chunk);
    const size_t read = in.read(chunk, want);
    if (read != want || out.write(chunk, read) != read) { ok = false; break; }
    copied += read;
  }
  in.close();
  out.close();
  if (!ok) return false;
  struct stat written;
  bool absent = false;
  return statPath(target, written, absent) && !absent && S_ISREG(written.st_mode) &&
         written.st_size >= 0 && size_t(written.st_size) == expected;
}
size_t slotBytes(const char* root, unsigned slot) {
  char path[kPathSize];
  struct stat info;
  bool missing = false;
  if (!buildPath(root, slot, path, sizeof(path)) || !statPath(path, info, missing) ||
      missing || !S_ISREG(info.st_mode) || info.st_size < 0) return 0;
  return size_t(info.st_size);
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
  // A RAM-only session has no filesystem identity to remount or validate.
  if (mode_.active && mode_.ramOnly) return LoadStatus::Loaded;
  if (!mounted_) {
    if (!LittleFS.begin(false, "/littlefs", 10, "spiffs")) return LoadStatus::NeedsProvisioning;
    mounted_ = true;
  }
  if (!capacity().valid) return LoadStatus::IoError;
  if (!initialized_) {
    initialized_ = true;
    root_ = kOfficialRoot;
    // The marker is the only boot-time authority. A read error is not proof
    // that it is absent, so fail closed rather than adopting an identity.
    const MarkerState marker = markerState();
    if (marker == MarkerState::Error) {
      initialized_ = false;
      return LoadStatus::IoError;
    }
    if (marker == MarkerState::Present) {
      mode_.active = true;
      root_ = kTestRoot;
    }
  }
  // Repeated calls only validate the root already selected; they must not drop
  // fault or seed state the user set in this session.
  if (!ensureDirectory(root_)) return LoadStatus::IoError;
  return LoadStatus::Loaded;
}

LoadStatus DeviceStore::prepare() {
  if (owner != this) return LoadStatus::NeedsProvisioning;
  if (mode_.active && mode_.ramOnly) return LoadStatus::Loaded;
  if (!mounted_) return LoadStatus::NeedsProvisioning;
  if (!capacity().valid) return LoadStatus::IoError;
  char path[kPathSize];
  struct stat info;
  bool missing = false;
  if (!buildDir(root_, path, sizeof(path)) || !statPath(path, info, missing) || missing)
    return LoadStatus::IoError;
  return S_ISDIR(info.st_mode) ? LoadStatus::Loaded : LoadStatus::IoError;
}

LoadResult DeviceStore::load(Save& out) {
  if (mode_.active && mode_.fault == TestFault::Load) return LoadResult(LoadStatus::IoError);
  if (!(mode_.active && mode_.ramOnly)) {
    if (owner != this) return LoadResult(LoadStatus::NeedsProvisioning);
    const LoadStatus ready = begin();
    if (ready != LoadStatus::Loaded) return LoadResult(ready);
  }
  if (mode_.active && mode_.ramOnly) {
    if (!ramSave_ || !ramHasSave_) return LoadResult(LoadStatus::Empty);
    out = *ramSave_;
    return LoadResult(LoadStatus::Loaded);
  }
  return store_.load(out);
}

CommitStatus DeviceStore::commit(const Save& candidate) {
  if (mode_.active && mode_.fault == TestFault::CommitBefore) return CommitStatus::IoError;
  if (!(mode_.active && mode_.ramOnly)) {
    const LoadStatus ready = begin();
    if (ready != LoadStatus::Loaded) return CommitStatus::IoError;
  }
  if (mode_.active && mode_.ramOnly) {
    if (!ramSave_) return CommitStatus::IoError;
    *ramSave_ = candidate;
    ramHasSave_ = true;
  } else {
    const CommitStatus result = store_.commit(candidate);
    if (result != CommitStatus::Committed) return result;
  }
  return mode_.active && mode_.fault == TestFault::CommitAfter ?
    CommitStatus::IoError : CommitStatus::Committed;
}

ReadStatus DeviceStore::read(unsigned slot, uint8_t* out, size_t cap, size_t& length) {
  length = 0;
  char path[kPathSize];
  if (!buildPath(root_, slot, path, sizeof(path))) return ReadStatus::IoError;
  struct stat info;
  bool missing = false;
  if (!statPath(path, info, missing)) return ReadStatus::IoError;
  if (missing) return ReadStatus::Missing;
  if (!S_ISREG(info.st_mode) || info.st_size < 0) return ReadStatus::IoError;
  File file = LittleFS.open(path, "r");
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
  if (mode_.active && mode_.ramOnly) return false;
  if (prepare() != LoadStatus::Loaded) return false;
  char path[kPathSize];
  if (!buildPath(root_, slot, path, sizeof(path))) return false;
  struct stat info;
  bool missing = false;
  if (!statPath(path, info, missing) || (!missing && !S_ISREG(info.st_mode))) return false;
  const StorageCapacity c = capacity();
  // Conservative: leave room for a complete copy-on-write candidate, not just its growth.
  if (!c.valid || c.availableBytes < length) return false;
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  const bool complete = file.write(bytes, length) == length;
  file.flush(); // Arduino API is void; FileStore proves the result by close/readback.
  file.close();
  return complete;
}

bool DeviceStore::seedSnapshot() {
  bool ok = clearSlots(kTestRoot);
  for (unsigned slot = 0; slot < 2; ++slot) {
    char source[kPathSize], target[kPathSize];
    if (!buildPath(kOfficialRoot, slot, source, sizeof(source)) ||
        !buildPath(kTestRoot, slot, target, sizeof(target)) ||
        !copyFile(source, target)) ok = false;
  }
  return ok;
}

bool DeviceStore::enterTest(TestSeed seed) {
  if (!mode_.active) {
    if (!mounted_) {
      // No filesystem identity can be persisted here: accept only a blank
      // single-save session that never touches LittleFS.
      if (seed != TestSeed::Blank) return false;
      ramSave_.reset(new (std::nothrow) Save());
      if (!ramSave_) return false;
      ramHasSave_ = false;
      mode_ = TestMode();
      mode_.active = true;
      mode_.ramOnly = true;
      store_.reset();
      return true;
    }
    if (!ensureDirectory(kTestRoot)) return false;
    // Write identity before seeding: a power loss may leave a blank test save,
    // but must never make the official runtime load the test directory.
    if (!writeActive()) {
      LittleFS.remove(kActivePath);
      return false;
    }
    mode_ = TestMode();
    mode_.active = true;
    root_ = kTestRoot;
    store_.reset();
  } else if (mode_.ramOnly) {
    if (seed != TestSeed::Blank) return false;
    ramHasSave_ = false;
    mode_.seedFailed = false;
    return true;
  }
  mode_.fault = TestFault::None;
  store_.reset();
  const bool seeded = seed == TestSeed::Snapshot ? seedSnapshot() : clearSlots(kTestRoot);
  mode_.seedFailed = !seeded;
  return true;
}

bool DeviceStore::leaveTest() {
  if (!mode_.active) return false;
  bool clean = true;
  if (!mode_.ramOnly && mounted_) {
    // Deactivate first. If the marker cannot be removed the session stays in
    // test mode so a restart cannot disagree with the visible UI.
    if (!removeFile(kActivePath)) return false;
    if (!clearSlots(kTestRoot)) clean = false;
    if (!removeDirectory(kTestRoot)) clean = false;
  }
  mode_ = TestMode();
  mode_.cleanupFailed = !clean;
  root_ = kOfficialRoot;
  ramSave_.reset();
  ramHasSave_ = false;
  store_.reset();
  if (mounted_ && !ensureDirectory(kOfficialRoot)) mode_.cleanupFailed = true;
  return true;
}

TestDiagnostics DeviceStore::diagnostics() const {
  TestDiagnostics d;
  d.mounted = mounted_;
  d.testMode = mode_.active;
  d.ramOnly = mode_.active && mode_.ramOnly;
  d.cleanupFailed = mode_.cleanupFailed;
  d.seedFailed = mode_.seedFailed;
  if (!mounted_) return d;
  d.officialBytes = slotBytes(kOfficialRoot, 0) + slotBytes(kOfficialRoot, 1);
  if (mode_.active && !mode_.ramOnly)
    d.testBytes = slotBytes(kTestRoot, 0) + slotBytes(kTestRoot, 1);
  const StorageCapacity c = capacity();
  d.capacityBytes = c.totalBytes;
  d.availableBytes = c.availableBytes;
  return d;
}

} }
