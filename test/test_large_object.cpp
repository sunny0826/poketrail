#include "large_object.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace {
bool failAllocation = false;
unsigned allocations = 0, frees = 0, constructed = 0, destroyed = 0;
struct Object {
  explicit Object(int initial) : value(initial) { ++constructed; }
  ~Object() { ++destroyed; }
  int value;
};
}
void* heap_caps_malloc(size_t size, uint32_t capabilities) {
  assert(capabilities == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  ++allocations;
  return failAllocation ? nullptr : std::malloc(size);
}
void heap_caps_free(void* memory) { ++frees; std::free(memory); }
int main() {
  {
    auto first = pokedex::makeLargeObject<Object>(649);
    assert(first && first->value == 649 && constructed == 1);
    auto second = std::move(first);
    assert(!first && second && allocations == 1 && frees == 0);
    second.reset();
    assert(destroyed == 1 && frees == 1);
  }
  failAllocation = true;
  auto missing = pokedex::makeLargeObject<Object>(251);
  assert(!missing && allocations == 2 && constructed == 1 && destroyed == 1 && frees == 1);
  std::puts("PASS large objects: PSRAM capability, ownership, destruction and allocation failure");
}
