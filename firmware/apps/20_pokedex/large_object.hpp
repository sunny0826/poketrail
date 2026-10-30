#pragma once
#include <memory>
#include <new>
#include <utility>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

namespace pokedex {
#if defined(ARDUINO_ARCH_ESP32)
template<class T> struct LargeObjectDeleter {
  void operator()(T* object) const {
    if (!object) return;
    object->~T();
    heap_caps_free(object);
  }
};
template<class T> using LargeObject = std::unique_ptr<T, LargeObjectDeleter<T>>;
#else
template<class T> using LargeObject = std::unique_ptr<T>;
#endif

// Large state stays in PSRAM on the C152, leaving internal RAM for I/O and stacks.
template<class T, class... Args> LargeObject<T> makeLargeObject(Args&&... args) {
#if defined(ARDUINO_ARCH_ESP32)
  void* memory = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!memory) return LargeObject<T>();
  return LargeObject<T>(new (memory) T(std::forward<Args>(args)...));
#else
  return LargeObject<T>(new (std::nothrow) T(std::forward<Args>(args)...));
#endif
}
}  // namespace pokedex
