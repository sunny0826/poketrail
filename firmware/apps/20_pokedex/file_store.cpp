#include "file_store.hpp"
#include "save_codec.hpp"
#include <cstring>
#include <new>
namespace pokedex { namespace adventure {
struct FileStore::Workspace {
  uint8_t bytes[2][codec::maxEncodedSize];
  uint8_t candidate[codec::maxEncodedSize];
  size_t length[2] = {0, 0};
  uint64_t sequence[2] = {0, 0};
  LoadStatus status[2] = {LoadStatus::Empty, LoadStatus::Empty};
  bool pending = false;
  bool initialPending = false;
  size_t pendingLength = 0;
  Save decoded;
};
FileStore::FileStore(SlotIO& io) : io_(io) {}
FileStore::~FileStore() {}
bool FileStore::allocate() {
  if (!work_) work_.reset(new (std::nothrow) Workspace());
  return bool(work_);
}
size_t FileStore::workspaceBytes() { return sizeof(Workspace); }
const char* FileStore::slotPath(unsigned slot) {
  return slot == 0 ? "/adventure/save-a.bin" :
         slot == 1 ? "/adventure/save-b.bin" : nullptr;
}
namespace {
bool unknownHeader(const uint8_t* p, size_t n) {
  return n >= codec::kHeaderSize && p[0] == 'A' && p[1] == 'V' &&
    p[2] == 'D' && p[3] == '1' &&
    (unsigned(p[4]) | (unsigned(p[5]) << 8)) != 1 &&
    (unsigned(p[4]) | (unsigned(p[5]) << 8)) != kSchemaVersion;
}
}
LoadResult FileStore::scan(int& current) {
  current = -1;
  const LoadStatus ready = io_.prepare();
  if (ready != LoadStatus::Loaded) return LoadResult(ready);
  if (!allocate()) return LoadResult(LoadStatus::IoError);
  Workspace& w = *work_;
  for (unsigned i = 0; i < 2; ++i) {
    w.length[i] = 0;
    const ReadStatus r = io_.read(i, w.bytes[i], codec::maxEncodedSize, w.length[i]);
    if (r == ReadStatus::Missing) w.status[i] = LoadStatus::Empty;
    else if (r == ReadStatus::IoError) w.status[i] = LoadStatus::IoError;
    else if (unknownHeader(w.bytes[i], w.length[i])) w.status[i] = LoadStatus::UnsupportedVersion;
    else if (r == ReadStatus::TooLarge || w.length[i] > codec::maxEncodedSize)
      w.status[i] = LoadStatus::Corrupt;
    else {
      const codec::DecodeStatus d = codec::decode(w.bytes[i], w.length[i], w.decoded);
      w.status[i] = d == codec::DecodeStatus::Ok ? LoadStatus::Loaded :
        d == codec::DecodeStatus::UnsupportedVersion ? LoadStatus::UnsupportedVersion :
        d == codec::DecodeStatus::IoError ? LoadStatus::IoError : LoadStatus::Corrupt;
      if (d == codec::DecodeStatus::Ok) w.sequence[i] = w.decoded.sequence;
    }
  }
  for (unsigned i = 0; i < 2; ++i)
    if (w.status[i] == LoadStatus::UnsupportedVersion) return LoadResult(LoadStatus::UnsupportedVersion);
  for (unsigned i = 0; i < 2; ++i)
    if (w.status[i] == LoadStatus::IoError) return LoadResult(LoadStatus::IoError);
  const bool a = w.status[0] == LoadStatus::Loaded, b = w.status[1] == LoadStatus::Loaded;
  if (a && b && w.sequence[0] == w.sequence[1] &&
      (w.length[0] != w.length[1] || std::memcmp(w.bytes[0], w.bytes[1], w.length[0]) != 0))
    return LoadResult(LoadStatus::Corrupt);
  if (a || b) {
    current = a && (!b || w.sequence[0] >= w.sequence[1]) ? 0 : 1;
    return LoadResult(LoadStatus::Loaded, w.status[1-current] == LoadStatus::Corrupt);
  }
  return LoadResult(w.status[0] == LoadStatus::Empty && w.status[1] == LoadStatus::Empty ?
                    LoadStatus::Empty : LoadStatus::Corrupt);
}
LoadResult FileStore::load(Save& out) {
  int current;
  const LoadResult result = scan(current);
  if (result.status != LoadStatus::Loaded) return result;
  const codec::DecodeStatus d = codec::decode(work_->bytes[current], work_->length[current], work_->decoded);
  if (d != codec::DecodeStatus::Ok) return LoadResult(LoadStatus::IoError);
  out = work_->decoded;
  return result;
}
CommitStatus FileStore::commit(const Save& candidate) {
  if (!allocate()) return CommitStatus::IoError;
  Workspace& w = *work_;
  size_t length = 0;
  uint8_t* encoded = w.pending ? w.bytes[0] : w.candidate;
  if (!codec::encode(candidate, encoded, sizeof(w.candidate), length)) return CommitStatus::Conflict;
  if (w.pending && (length != w.pendingLength || std::memcmp(encoded, w.candidate, length) != 0))
    return CommitStatus::Conflict;
  int current;
  const LoadResult result = scan(current);
  const bool retryInitial = w.pending && w.initialPending && result.status == LoadStatus::Corrupt &&
    w.status[0] == LoadStatus::Corrupt && w.status[1] == LoadStatus::Empty;
  if (result.status != LoadStatus::Loaded && result.status != LoadStatus::Empty && !retryInitial)
    return CommitStatus::IoError;
  if (current >= 0) {
    if (candidate.sequence == w.sequence[current]) {
      if (length != w.length[current] || std::memcmp(w.candidate, w.bytes[current], length) != 0)
        return CommitStatus::Conflict;
      w.pending = false;
      return CommitStatus::Committed;
    }
    if (w.sequence[current] == UINT64_MAX || candidate.sequence != w.sequence[current] + 1)
      return CommitStatus::Conflict;
  } else if (candidate.sequence != 1) return CommitStatus::Conflict;
  const unsigned target = current < 0 ? 0 : unsigned(1-current);
  w.pending = true;
  w.initialPending = current < 0;
  w.pendingLength = length;
  if (!io_.write(target, w.candidate, length)) return CommitStatus::IoError;
  size_t actual = 0;
  if (io_.read(target, w.bytes[target], codec::maxEncodedSize, actual) != ReadStatus::Ok ||
      actual != length || std::memcmp(w.bytes[target], w.candidate, length) != 0 ||
      codec::decode(w.bytes[target], actual, w.decoded) != codec::DecodeStatus::Ok)
    return CommitStatus::IoError;
  w.pending = false;
  return CommitStatus::Committed;
}
} }
