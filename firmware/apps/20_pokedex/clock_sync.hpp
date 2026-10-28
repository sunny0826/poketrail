#pragma once
#include <cstddef>
#include <cstdint>

namespace pokedex { namespace clock_sync {

// Bounded, line-oriented host protocol used after flashing:
//   PTC?                          read-only status
//   PTC <id> <utcSeconds> <offset>  request one calibration transaction
// Replies are single lines: PTC q, PTC s or PTC e with key=value fields.
constexpr size_t kMaxLineBytes = 80;

enum class CommandKind : uint8_t { None, Query, Set, Error };
enum class ParseError : uint8_t {
  None, Empty, Overflow, InvalidCommand, InvalidId, InvalidUtc, InvalidOffset, Garbage
};

struct Request {
  CommandKind kind = CommandKind::None;
  ParseError error = ParseError::None;
  uint32_t id = 0;
  int64_t utcSeconds = 0;
  int16_t offsetMinutes = 0;
};

// Incremental parser. Bytes may be split arbitrarily across main-loop passes;
// overlong, truncated or polluted lines are rejected without allocating.
class Parser {
 public:
  Request feed(uint8_t byte);
  void reset();
 private:
  Request finish();
  char line_[kMaxLineBytes + 1] = {};
  size_t length_ = 0;
  bool overflow_ = false;
  bool invalid_ = false;
};

struct Status {
  bool testMode = false;
  bool ready = false;
  bool busy = false;
  bool savePending = false;
  bool rtcValid = false;
  bool rtcLowVoltage = false;
  bool rtcIoError = false;
  int64_t utcSeconds = 0;
  int16_t offsetMinutes = 0;
  uint32_t dayKey = 0;
  bool calibrated = false;
  bool trusted = false;
  bool pending = false;
  bool anomaly = false;
  uint64_t transactionId = 0;
  uint64_t sequence = 0;
};

const char* errorName(ParseError error);

// Each formatter returns the line length, or 0 when the buffer is too small.
// It never writes a truncated line.
size_t formatQuery(char* out, size_t capacity, const Status& status);
size_t formatSet(char* out, size_t capacity, uint32_t id, const char* result,
                 const Status& status);
size_t formatError(char* out, size_t capacity, const char* result);

} }
