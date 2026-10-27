#pragma once
#include <cstddef>
#include <cstdint>

namespace pokedex { namespace adventure {

// Test saves live beside the official slots. The marker records whether the
// next boot should use the test root; it is never consulted by the official
// store itself.
enum class TestSeed : uint8_t { Snapshot, Blank };
enum class TestFault : uint8_t { None, CommitBefore, CommitAfter, Load };

constexpr int64_t kTestDefaultUtc = 1792627200;

// Pure state for a test session. Clock simulation only affects the wrapped
// Platform; the hardware RTC is never written through this structure.
struct TestMode {
  bool active = false;
  bool ramOnly = false;
  bool clockStarted = false;
  uint32_t clockBaseMs = 0;
  int64_t clockBaseUtc = 0;
  int16_t clockOffsetMinutes = 480;
  TestFault fault = TestFault::None;
  bool cleanupFailed = false;
  bool seedFailed = false;

  void beginSimulation(uint32_t nowMs, int64_t utcSeconds, int16_t offsetMinutes = 480);
  void endSimulation();
  int64_t utcAt(uint32_t nowMs) const;
  void setUtc(uint32_t nowMs, int64_t utcSeconds);
  void setOffset(int16_t offsetMinutes);
};

struct TestDiagnostics {
  bool mounted = false;
  bool testMode = false;
  bool ramOnly = false;
  bool cleanupFailed = false;
  bool seedFailed = false;
  size_t officialBytes = 0;
  size_t testBytes = 0;
  size_t capacityBytes = 0;
  size_t availableBytes = 0;
};

// Implemented by the storage owner. UI and Runtime only use this interface so
// the official path never needs test-mode branches.
class TestModeHost {
 public:
  virtual ~TestModeHost() {}
  virtual bool testMode() const = 0;
  virtual bool ramOnly() const = 0;
  virtual bool enterTest(TestSeed seed) = 0;
  virtual bool leaveTest() = 0;
  virtual TestMode& mode() = 0;
  virtual void setFault(TestFault fault) = 0;
  virtual TestFault fault() const = 0;
  virtual TestDiagnostics diagnostics() const = 0;
};

} }

namespace pokedex { namespace app {
// Implemented by the firmware composition root: checkpoint the official
// runtime, rebuild it on the selected store root, and return whether the mode
// changed. UI tasks call these instead of touching DeviceStore directly.
bool enterTestMode(adventure::TestSeed seed);
bool leaveTestMode();
} }
