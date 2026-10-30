#include "../firmware/apps/20_pokedex/file_store.hpp"
#include "../firmware/apps/20_pokedex/save_codec.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace pokedex::adventure;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
struct MemoryIO : SlotIO {
  std::vector<uint8_t> files[2];
  bool present[2] = {false, false};
  LoadStatus ready = LoadStatus::Loaded;
  int badRead = -1, failReadCall = -1, reads = 0, writes = 0;
  int lastWrite = -1;
  size_t truncate = SIZE_MAX;
  bool rejectWrite = false, failAfterWrite = false, mutateWrite = false;
  LoadStatus prepare() override { return ready; }
  ReadStatus read(unsigned s, uint8_t* out, size_t cap, size_t& n) override {
    ++reads;
    if (int(s) == badRead || reads == failReadCall) return ReadStatus::IoError;
    if (!present[s]) return ReadStatus::Missing;
    n = files[s].size();
    std::copy(files[s].begin(), files[s].begin() + std::min(cap, n), out);
    return n > cap ? ReadStatus::TooLarge : ReadStatus::Ok;
  }
  bool write(unsigned s, const uint8_t* p, size_t n) override {
    ++writes; lastWrite = int(s);
    if (rejectWrite) return false;
    present[s] = true;
    files[s].assign(p, p + std::min(n, truncate));
    if (mutateWrite && !files[s].empty()) files[s].back() ^= 1;
    return truncate >= n && !failAfterWrite;
  }
  void put(unsigned s, const Save& save) {
    files[s].resize(codec::maxEncodedSize); size_t n = 0;
    CHECK(codec::encode(save, files[s].data(), files[s].size(), n));
    files[s].resize(n); present[s] = true;
  }
};
static void checksum(std::vector<uint8_t>& b) {
  std::vector<uint8_t> covered(b.begin(), b.begin()+18);
  covered.insert(covered.end(), b.begin()+22, b.end());
  uint32_t c = codec::crc32(covered.data(), covered.size());
  for (unsigned i = 0; i < 4; ++i) b[18+i] = uint8_t(c >> (8*i));
}
static void selection() {
  Save one; one.sequence = 1; one.encounterTarget = 300;
  Save two = one; two.sequence = 2;
  Save out; out.sequence = 77;
  MemoryIO io; FileStore store(io);
  CHECK(store.load(out).status == LoadStatus::Empty && out.sequence == 77);
  io.ready = LoadStatus::NeedsProvisioning;
  CHECK(store.load(out).status == LoadStatus::NeedsProvisioning);
  CHECK(store.commit(one) == CommitStatus::IoError && io.writes == 0);
  io.ready = LoadStatus::IoError;
  CHECK(store.load(out).status == LoadStatus::IoError);
  io.ready = LoadStatus::Loaded;
  io.put(0, one);
  CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 1);
  io.put(1, two);
  CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 2);
  io.files[1].back() ^= 1;
  LoadResult r = store.load(out);
  CHECK(r.status == LoadStatus::Loaded && r.degraded && out.sequence == 1);
  io.files[0].back() ^= 1;
  CHECK(store.load(out).status == LoadStatus::Corrupt);
  CHECK(store.commit(two) == CommitStatus::IoError && io.writes == 0);
  io.present[0] = false;
  CHECK(store.load(out).status == LoadStatus::Corrupt);
  io.put(0, one); io.put(1, one);
  CHECK(store.load(out).status == LoadStatus::Loaded && !store.load(out).degraded);
  one.bootGeneration = 1;
  one.totalSteps = one.undatedSteps = one.explorationCredit = 1;
  io.put(1, one);
  CHECK(store.load(out).status == LoadStatus::Corrupt);
  CHECK(store.commit(two) == CommitStatus::IoError);
  io.put(1, two); io.files[1][4] = 99; checksum(io.files[1]);
  CHECK(store.load(out).status == LoadStatus::UnsupportedVersion);
  CHECK(store.commit(two) == CommitStatus::IoError);
  io.files[1].resize(codec::maxEncodedSize+1);
  CHECK(store.load(out).status == LoadStatus::UnsupportedVersion);
  io.files[1][4] = 1;
  CHECK(store.load(out).status == LoadStatus::Loaded && store.load(out).degraded);
  io.badRead = 1;
  CHECK(store.load(out).status == LoadStatus::IoError);
  CHECK(store.commit(two) == CommitStatus::IoError);
  io.badRead = -1; io.put(1, two);
  io.files[1][26] = 255; checksum(io.files[1]);
  r = store.load(out);
  CHECK(r.status == LoadStatus::Loaded && r.degraded && out.sequence == 1);
  CHECK(store.commit(two) == CommitStatus::Committed);
}
static void unsupportedVersions() {
  // The header gate must reject unknown schemas before the TooLarge branch so an
  // oversized slot cannot be mistaken for a corrupt-but-overwritable copy.
  Save one; one.sequence = 1; one.encounterTarget = 300;
  Save two = one; two.sequence = 2;
  Save out;
  MemoryIO io; FileStore store(io);
  io.put(0, one);
  io.put(1, two); io.files[1][4] = 0; checksum(io.files[1]);
  CHECK(store.load(out).status == LoadStatus::UnsupportedVersion);
  CHECK(store.commit(two) == CommitStatus::IoError && io.writes == 0);
  io.put(1, two); io.files[1].resize(codec::maxEncodedSize+1); io.files[1][4] = 0;
  CHECK(store.load(out).status == LoadStatus::UnsupportedVersion);
  CHECK(store.commit(two) == CommitStatus::IoError && io.writes == 0);
  io.files[1][4] = 99;
  CHECK(store.load(out).status == LoadStatus::UnsupportedVersion);
  CHECK(store.commit(two) == CommitStatus::IoError && io.writes == 0);
  io.files[1][4] = 1;
  CHECK(store.load(out).status == LoadStatus::Loaded && store.load(out).degraded);
  CHECK(store.commit(two) == CommitStatus::Committed && io.lastWrite == 1);
}
static void commits() {
  Save one; one.sequence = 1; one.encounterTarget = 300;
  Save two = one; two.sequence = 2;
  Save out;
  MemoryIO io; FileStore store(io);
  Save invalid = one; invalid.route = Route(255);
  CHECK(store.commit(invalid) == CommitStatus::Conflict && io.writes == 0);
  CHECK(store.commit(two) == CommitStatus::Conflict && io.writes == 0);
  CHECK(store.commit(one) == CommitStatus::Committed && io.lastWrite == 0);
  const std::vector<uint8_t> old = io.files[0];
  CHECK(store.commit(one) == CommitStatus::Committed && io.writes == 1);
  Save conflict = one; conflict.stepsIncomplete = true;
  CHECK(store.commit(conflict) == CommitStatus::Conflict && io.writes == 1);
  Save skip = two; skip.sequence = 3;
  CHECK(store.commit(skip) == CommitStatus::Conflict);
  io.rejectWrite = true;
  CHECK(store.commit(two) == CommitStatus::IoError && io.files[0] == old);
  io.rejectWrite = false; io.truncate = 100;
  CHECK(store.commit(two) == CommitStatus::IoError && io.files[0] == old);
  CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 1);
  io.truncate = SIZE_MAX;
  CHECK(store.commit(two) == CommitStatus::Committed && io.lastWrite == 1);
  FileStore reboot(io);
  CHECK(reboot.load(out).status == LoadStatus::Loaded && out.sequence == 2);
  CHECK(reboot.commit(one) == CommitStatus::Conflict);
  skip.sequence = 3; io.failAfterWrite = true;
  CHECK(store.commit(skip) == CommitStatus::IoError);
  int writes = io.writes;
  CHECK(store.commit(skip) == CommitStatus::Committed && io.writes == writes);
  io.failAfterWrite = false;
  skip.sequence = 4; io.failReadCall = io.reads + 3;
  CHECK(store.commit(skip) == CommitStatus::IoError);
  writes = io.writes;
  CHECK(store.commit(skip) == CommitStatus::Committed && io.writes == writes);
  skip.sequence = 5; io.mutateWrite = true;
  CHECK(store.commit(skip) == CommitStatus::IoError);
  CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 4);
  io.mutateWrite = false;
  CHECK(store.commit(skip) == CommitStatus::Committed);
  CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 5);
  skip.sequence = UINT64_MAX; io.put(0, skip); io.present[1] = false;
  CHECK(store.commit(one) == CommitStatus::Conflict);
}
static void truncations() {
  Save a; a.sequence = 1; a.encounterTarget = 300;
  Save b = a; b.sequence = 2;
  MemoryIO encoded; encoded.put(0, b);
  const size_t n = encoded.files[0].size();
  const size_t cuts[] = {0, 1, 4, 6, 14, 18, 21, 22, 100, n/2, n-1};
  for (size_t cut : cuts) {
    MemoryIO io; io.put(0, a); io.truncate = cut;
    FileStore store(io); Save out;
    CHECK(store.commit(b) == CommitStatus::IoError);
    CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 1);
    io.truncate = SIZE_MAX;
    CHECK(store.commit(b) == CommitStatus::Committed);
    CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 2);
  }
  {
    MemoryIO io; FileStore store(io); io.truncate = 100;
    CHECK(store.commit(a) == CommitStatus::IoError);
    Save changed = a; changed.stepsIncomplete = true;
    CHECK(store.commit(changed) == CommitStatus::Conflict);
    FileStore reboot(io); Save out;
    CHECK(reboot.load(out).status == LoadStatus::Corrupt);
    CHECK(reboot.commit(a) == CommitStatus::IoError);
    io.truncate = SIZE_MAX;
    CHECK(store.commit(a) == CommitStatus::Committed);
    CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 1);
  }
  std::printf("encoded_bytes=%zu two_slots=%zu sizeof_Save=%zu workspace_bytes=%zu peak_owned_heap=%zu\n",
              n, 2*n, sizeof(Save), FileStore::workspaceBytes(), FileStore::workspaceBytes()+sizeof(Save));
}
int main() {
  selection(); unsupportedVersions(); commits(); truncations();
  CHECK(FileStore::slotPath(2) == nullptr);
  std::printf("PASS file_store: %d checks\n", checks);
}
