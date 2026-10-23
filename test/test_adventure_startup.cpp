#include "adventure.hpp"
#include <cstdio>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
using namespace pokedex::adventure;
namespace {
struct EmptyStore : Store {
  LoadResult load(Save&) override { return LoadResult(LoadStatus::Empty); }
  CommitStatus commit(const Save&) override { return CommitStatus::Committed; }
};
EmptyStore store;
Engine engine(store);
void* boot(void*) {
  const auto result = engine.begin();
  std::printf("startup result=%d ready=%d\n", int(result), engine.ready());
  return nullptr;
}
}
int main() {
  const rlimit noCore = {0, 0};
  if (setrlimit(RLIMIT_CORE, &noCore)) return 2;
  const size_t bytes = 16384, guard = 65536;
  void* region = mmap(nullptr, bytes + guard * 2, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (region == MAP_FAILED) return 2;
  void* stack = static_cast<char*>(region) + guard;
  if (mprotect(stack, bytes, PROT_READ | PROT_WRITE)) return 2;
  pthread_attr_t attr;
  if (pthread_attr_init(&attr)) return 2;
  if (pthread_attr_setstack(&attr, stack, bytes)) return 2;
  pthread_t thread;
  const int created = pthread_create(&thread, &attr, boot, nullptr);
  pthread_attr_destroy(&attr);
  if (created || pthread_join(thread, nullptr)) return 2;
  munmap(region, bytes + guard * 2);
  std::printf("startup fits protected %zu-byte host stack; target stack measured separately\n", bytes);
  return engine.ready() ? 0 : 1;
}
