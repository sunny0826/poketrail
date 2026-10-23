#pragma once
#include <stdint.h>

namespace pokedex { namespace device_clock {
struct Reading {
  int64_t utcSeconds;
  bool valid;
  bool lowVoltage;
  bool ioError;
  bool unavailable;
  bool invalidDate;
  bool verificationFailed;
};
} }
