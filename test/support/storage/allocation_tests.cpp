#include "../../../firmware/apps/20_pokedex/file_store.hpp"
#include "../../../firmware/apps/20_pokedex/save_codec.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
using namespace pokedex::adventure;
static int budget = -1;
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  if (budget == 0) return nullptr;
  if (budget > 0) --budget;
  try { return ::operator new(n); } catch (...) { return nullptr; }
}
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
struct IO : SlotIO {
  uint8_t bytes[codec::maxEncodedSize]; size_t length = 0; int writes = 0;
  LoadStatus prepare() override { return LoadStatus::Loaded; }
  ReadStatus read(unsigned s, uint8_t* out, size_t, size_t& n) override {
    if (s) return ReadStatus::Missing;
    n = length; std::memcpy(out, bytes, n); return ReadStatus::Ok;
  }
  bool write(unsigned, const uint8_t*, size_t) override { ++writes; return true; }
};
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"allocation line %d\n",__LINE__); std::exit(1); } } while (0)
int main() {
  IO io; Save a; a.sequence = 1; a.encounterTarget = 300; Save out; out.sequence = 99;
  CHECK(codec::encode(a, io.bytes, sizeof(io.bytes), io.length));
  {
    FileStore store(io); budget = 0;
    CHECK(store.load(out).status == LoadStatus::IoError && out.sequence == 99);
    CHECK(store.commit(a) == CommitStatus::IoError && io.writes == 0);
    budget = -1;
    CHECK(store.load(out).status == LoadStatus::Loaded && out.sequence == 1);
  }
  {
    FileStore store(io); budget = 1; // workspace succeeds, real codec allocation fails
    CHECK(store.load(out).status == LoadStatus::IoError);
    CHECK(store.commit(a) == CommitStatus::IoError && io.writes == 0);
    budget = -1;
    CHECK(store.load(out).status == LoadStatus::Loaded);
    CHECK(store.commit(a) == CommitStatus::Committed && io.writes == 0);
  }
  std::printf("PASS allocation: %d checks (real codec OOM preserves slots)\n",checks);
}
