#include "clock_sync.hpp"

#include <cstdio>
#include <cstring>

#include "calendar.hpp"

namespace pokedex { namespace clock_sync {
namespace {

struct Token {
  const char* text = nullptr;
  size_t length = 0;
};

Request failure(ParseError error) {
  Request request;
  request.kind = CommandKind::Error;
  request.error = error;
  return request;
}

bool takeToken(const char*& cursor, Token& token, bool& trailing) {
  if (*cursor == 0 || *cursor == ' ') return false;
  token.text = cursor;
  while (*cursor && *cursor != ' ') ++cursor;
  token.length = size_t(cursor - token.text);
  if (*cursor == ' ') {
    ++cursor;
    if (*cursor == 0) trailing = true;
  }
  return true;
}

bool parseDigits(const Token& token, uint64_t maximum, uint64_t& value) {
  if (!token.length) return false;
  uint64_t result = 0;
  for (size_t i = 0; i < token.length; ++i) {
    const char c = token.text[i];
    if (c < '0' || c > '9') return false;
    const uint64_t digit = uint64_t(c - '0');
    if (result > (maximum - digit) / 10) return false;
    result = result * 10 + digit;
  }
  value = result;
  return true;
}

bool parseId(const Token& token, uint32_t& id) {
  uint64_t value = 0;
  if (!parseDigits(token, UINT32_MAX, value) || value == 0) return false;
  id = uint32_t(value);
  return true;
}

bool parseOffset(const Token& token, int16_t& offset) {
  Token digits = token;
  bool negative = false;
  if (digits.length && digits.text[0] == '-') {
    negative = true;
    ++digits.text;
    --digits.length;
  }
  uint64_t value = 0;
  if (!parseDigits(digits, 840, value)) return false;
  if (negative) {
    if (value > 720) return false;
    offset = int16_t(-int32_t(value));
  } else {
    offset = int16_t(value);
  }
  return true;
}

bool parseUtc(const Token& token, int64_t& utcSeconds) {
  uint64_t value = 0;
  if (!parseDigits(token, 4102444799ULL, value) || value < 946684800ULL) return false;
  utcSeconds = int64_t(value);
  return true;
}

Request parseLine(const char* line) {
  if (std::strcmp(line, "PTC?") == 0) {
    Request request;
    request.kind = CommandKind::Query;
    return request;
  }
  if (std::strncmp(line, "PTC ", 4) != 0) return failure(ParseError::InvalidCommand);
  const char* cursor = line + 4;
  Token idToken;
  Token utcToken;
  Token offsetToken;
  bool trailing = false;
  if (!takeToken(cursor, idToken, trailing)) return failure(ParseError::InvalidCommand);
  if (!takeToken(cursor, utcToken, trailing)) return failure(ParseError::InvalidCommand);
  if (!takeToken(cursor, offsetToken, trailing)) return failure(ParseError::InvalidCommand);
  if (trailing || *cursor != 0) return failure(ParseError::Garbage);
  Request request;
  if (!parseId(idToken, request.id)) return failure(ParseError::InvalidId);
  if (!parseUtc(utcToken, request.utcSeconds)) return failure(ParseError::InvalidUtc);
  if (!parseOffset(offsetToken, request.offsetMinutes)) return failure(ParseError::InvalidOffset);
  calendar::CivilTime local;
  if (!calendar::fromUtc(request.utcSeconds, request.offsetMinutes, local))
    return failure(ParseError::InvalidUtc);
  request.kind = CommandKind::Set;
  return request;
}

bool writeLine(char* out, size_t capacity, const char* text) {
  const size_t length = std::strlen(text);
  if (length + 1 > capacity) return false;
  std::memcpy(out, text, length + 1);
  return true;
}

const char* modeName(bool testMode) { return testMode ? "test" : "official"; }

size_t finishLine(char* out, size_t capacity, char* line, size_t lineSize, int length) {
  if (length <= 0 || size_t(length) >= lineSize) return 0;
  return writeLine(out, capacity, line) ? size_t(length) : 0;
}

}  // namespace

Request Parser::feed(uint8_t byte) {
  Request none;
  if (byte == '\n') return finish();
  // Terminals may send CRLF; a CR never changes the payload.
  if (byte == '\r') return none;
  if (byte < 0x20 || byte > 0x7e) {
    invalid_ = true;
    return none;
  }
  if (overflow_ || invalid_) return none;
  if (length_ >= kMaxLineBytes) {
    overflow_ = true;
    return none;
  }
  line_[length_++] = char(byte);
  return none;
}

void Parser::reset() {
  length_ = 0;
  overflow_ = false;
  invalid_ = false;
}

Request Parser::finish() {
  Request request;
  if (overflow_) request = failure(ParseError::Overflow);
  else if (invalid_) request = failure(ParseError::Garbage);
  else if (length_ == 0) request = failure(ParseError::Empty);
  else {
    line_[length_] = 0;
    request = parseLine(line_);
  }
  reset();
  return request;
}

const char* errorName(ParseError error) {
  switch (error) {
    case ParseError::Empty: return "empty";
    case ParseError::Overflow: return "overflow";
    case ParseError::InvalidCommand: return "command";
    case ParseError::InvalidId: return "id";
    case ParseError::InvalidUtc: return "utc";
    case ParseError::InvalidOffset: return "offset";
    case ParseError::Garbage: return "garbage";
    case ParseError::None: break;
  }
  return "none";
}

size_t formatQuery(char* out, size_t capacity, const Status& status) {
  char line[224];
  const int length = std::snprintf(
      line, sizeof line,
      "PTC q mode=%s rtc=%u low=%u io=%u utc=%lld off=%d day=%lu cal=%u tr=%u "
      "pend=%u anom=%u ready=%u busy=%u save=%u tx=%llu seq=%llu",
      modeName(status.testMode), unsigned(status.rtcValid), unsigned(status.rtcLowVoltage),
      unsigned(status.rtcIoError), static_cast<long long>(status.utcSeconds),
      int(status.offsetMinutes), static_cast<unsigned long>(status.dayKey),
      unsigned(status.calibrated), unsigned(status.trusted), unsigned(status.pending),
      unsigned(status.anomaly), unsigned(status.ready), unsigned(status.busy),
      unsigned(status.savePending), static_cast<unsigned long long>(status.transactionId),
      static_cast<unsigned long long>(status.sequence));
  return finishLine(out, capacity, line, sizeof line, length);
}

size_t formatSet(char* out, size_t capacity, uint32_t id, const char* result,
                 const Status& status) {
  char line[224];
  const int length = std::snprintf(
      line, sizeof line,
      "PTC s id=%u result=%s mode=%s rtc=%u low=%u io=%u utc=%lld off=%d day=%lu "
      "cal=%u tr=%u pend=%u anom=%u ready=%u busy=%u save=%u tx=%llu seq=%llu",
      unsigned(id), result ? result : "error", modeName(status.testMode),
      unsigned(status.rtcValid), unsigned(status.rtcLowVoltage), unsigned(status.rtcIoError),
      static_cast<long long>(status.utcSeconds), int(status.offsetMinutes),
      static_cast<unsigned long>(status.dayKey), unsigned(status.calibrated),
      unsigned(status.trusted), unsigned(status.pending), unsigned(status.anomaly),
      unsigned(status.ready), unsigned(status.busy), unsigned(status.savePending),
      static_cast<unsigned long long>(status.transactionId),
      static_cast<unsigned long long>(status.sequence));
  return finishLine(out, capacity, line, sizeof line, length);
}

size_t formatError(char* out, size_t capacity, const char* result) {
  char line[96];
  const int length = std::snprintf(line, sizeof line, "PTC e result=%s",
                                   result ? result : "error");
  return finishLine(out, capacity, line, sizeof line, length);
}

} }
