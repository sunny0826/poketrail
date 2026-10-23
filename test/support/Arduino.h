#pragma once

#include <cstdint>

extern uint32_t host_time_ms;
inline uint32_t millis() { return host_time_ms; }
inline uint32_t micros() { return host_time_ms * 1000; }
inline void delay(uint32_t ms) { host_time_ms += ms; }

struct HostSerial {
  template <typename... Args>
  void printf(const char*, Args...) {}
};
extern HostSerial Serial;
