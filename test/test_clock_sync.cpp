// 主机串口校时协议的最小回归：分片、边界、拒绝路径与回执格式。
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "clock_sync.hpp"

using namespace pokedex::clock_sync;

namespace {
unsigned checks = 0;

void check(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

Request feed(Parser& parser, const char* text) {
  Request request;
  for (const char* p = text; *p; ++p) {
    const Request next = parser.feed(static_cast<uint8_t>(*p));
    if (next.kind != CommandKind::None) request = next;
  }
  return request;
}

Request feedBytes(Parser& parser, const char* text, size_t count) {
  Request request;
  for (size_t i = 0; i < count; ++i) {
    const Request next = parser.feed(static_cast<uint8_t>(text[i]));
    if (next.kind != CommandKind::None) request = next;
  }
  return request;
}

bool errorIs(const Request& request, ParseError error) {
  return request.kind == CommandKind::Error && request.error == error;
}

Status sampleStatus() {
  Status status;
  status.testMode = false;
  status.ready = true;
  status.busy = false;
  status.savePending = false;
  status.rtcValid = true;
  status.rtcLowVoltage = false;
  status.rtcIoError = false;
  status.utcSeconds = 1792627200;
  status.offsetMinutes = 480;
  status.dayKey = 20261022;
  status.calibrated = true;
  status.trusted = true;
  status.pending = false;
  status.anomaly = false;
  status.transactionId = 42;
  status.sequence = 99;
  return status;
}
}

int main() {
  {
    Parser parser;
    Request request;
    request = parser.feed('P'); check(request.kind == CommandKind::None, "query waits for a complete line");
    request = parser.feed('T'); check(request.kind == CommandKind::None, "query accepts the second prefix byte");
    request = parser.feed('C'); check(request.kind == CommandKind::None, "query accepts the third prefix byte");
    request = parser.feed('?'); check(request.kind == CommandKind::None, "query waits for the terminator");
    request = parser.feed('\n'); check(request.kind == CommandKind::Query, "fragmented query parses");
  }
  {
    Parser parser;
    Request request = feed(parser, "PTC?\r\n");
    check(request.kind == CommandKind::Query, "CRLF query parses");
    request = feed(parser, "PTC?\n");
    check(request.kind == CommandKind::Query, "parser recovers after a completed line");
  }
  {
    Parser parser;
    Request request = feed(parser, "PTC 42 1792627200 480\n");
    check(request.kind == CommandKind::Set, "valid set parses");
    check(request.id == 42 && request.utcSeconds == 1792627200 && request.offsetMinutes == 480,
          "set carries id, UTC seconds and offset");
    request = feed(parser, "PTC 7 1792627200 -720\r\n");
    check(request.kind == CommandKind::Set && request.offsetMinutes == -720, "negative offset parses");
    request = feed(parser, "PTC 8 1792627200 840\n");
    check(request.kind == CommandKind::Set && request.offsetMinutes == 840, "maximum offset parses");
  }
  {
    Parser parser;
    check(errorIs(feed(parser, "PTC?\n\n"), ParseError::Empty), "blank line is rejected");
    check(errorIs(feed(parser, "PTC\n"), ParseError::InvalidCommand), "missing fields are rejected");
    check(errorIs(feed(parser, "PTC 42 1792627200\n"), ParseError::InvalidCommand), "truncated set is rejected");
    check(errorIs(feed(parser, "PTC  42 1792627200 480\n"), ParseError::InvalidCommand), "double separator is rejected");
    check(errorIs(feed(parser, "PTC 42 1792627200 480 extra\n"), ParseError::Garbage), "fourth field is rejected");
    check(errorIs(feed(parser, "PTC 42 1792627200 480\t\n"), ParseError::Garbage), "tab terminator is rejected");
    check(errorIs(feed(parser, "PTC 42 1792627200 480 \n"), ParseError::Garbage), "trailing space is rejected");
    check(errorIs(feed(parser, "PTC?x\n"), ParseError::InvalidCommand), "query suffix is rejected");
    check(errorIs(feed(parser, "CLK 42 1792627200 480\n"), ParseError::InvalidCommand), "unknown command is rejected");
  }
  {
    Parser parser;
    check(errorIs(feed(parser, "PTC 0 1792627200 480\n"), ParseError::InvalidId), "zero id is rejected");
    check(errorIs(feed(parser, "PTC -1 1792627200 480\n"), ParseError::InvalidId), "negative id is rejected");
    check(errorIs(feed(parser, "PTC 4294967296 1792627200 480\n"), ParseError::InvalidId), "overflowing id is rejected");
    check(errorIs(feed(parser, "PTC 42x 1792627200 480\n"), ParseError::InvalidId), "non-digit id is rejected");
    check(errorIs(feed(parser, "PTC 42 -1 480\n"), ParseError::InvalidUtc), "negative UTC is rejected");
    check(errorIs(feed(parser, "PTC 42 946684799 480\n"), ParseError::InvalidUtc), "UTC below the calendar range is rejected");
    check(errorIs(feed(parser, "PTC 42 4102444800 480\n"), ParseError::InvalidUtc), "UTC above the calendar range is rejected");
    check(errorIs(feed(parser, "PTC 42 999999999999999999999 480\n"), ParseError::InvalidUtc), "overflowing UTC is rejected");
    check(errorIs(feed(parser, "PTC 42 1792627200 900\n"), ParseError::InvalidOffset), "offset above range is rejected");
    check(errorIs(feed(parser, "PTC 42 1792627200 -721\n"), ParseError::InvalidOffset), "offset below range is rejected");
    check(errorIs(feed(parser, "PTC 42 1792627200 48x\n"), ParseError::InvalidOffset), "non-digit offset is rejected");
  }
  {
    Parser parser;
    char longLine[kMaxLineBytes + 8];
    std::memset(longLine, 'A', sizeof longLine);
    Request request = feedBytes(parser, longLine, sizeof longLine);
    check(request.kind == CommandKind::None, "overlong line is discarded before its terminator");
    request = parser.feed('\n');
    check(errorIs(request, ParseError::Overflow), "overlong line is rejected");
    request = feed(parser, "PTC?\n");
    check(request.kind == CommandKind::Query, "parser recovers after overflow");
    request = parser.feed(0);
    check(request.kind == CommandKind::None, "control byte is discarded until the line ends");
    request = parser.feed('\n');
    check(errorIs(request, ParseError::Garbage), "control byte rejects the line");
    request = feed(parser, "PTC?\n");
    check(request.kind == CommandKind::Query, "parser recovers after a control byte");
  }
  {
    const Status status = sampleStatus();
    char line[256];
    size_t size = formatQuery(line, sizeof line, status);
    check(size > 0 && std::strncmp(line, "PTC q ", 6) == 0, "query reply has a stable prefix");
    check(std::strstr(line, " mode=official ") && std::strstr(line, " rtc=1 ") &&
          std::strstr(line, " cal=1 ") && std::strstr(line, " tr=1 ") &&
          std::strstr(line, " pend=0 ") && std::strstr(line, " tx=42 ") &&
          std::strstr(line, " seq=99"), "query reply exposes calibration, trust, pending and counters");
    check(formatQuery(line, 16, status) == 0, "short buffer is rejected instead of truncated");
    size = formatSet(line, sizeof line, 42, "ok", status);
    check(size > 0 && std::strncmp(line, "PTC s ", 6) == 0, "set reply has a stable prefix");
    check(std::strstr(line, " id=42 ") && std::strstr(line, " result=ok ") &&
          std::strstr(line, " tr=1 ") && std::strstr(line, " utc=1792627200 "),
          "set reply carries the request id, result and readback");
    size = formatError(line, sizeof line, errorName(ParseError::Overflow));
    check(size > 0 && std::strcmp(line, "PTC e result=overflow") == 0, "parse error reply is explicit");
    check(std::strcmp(errorName(ParseError::Garbage), "garbage") == 0 &&
          std::strcmp(errorName(ParseError::InvalidUtc), "utc") == 0,
          "parse errors have stable names");
  }
  std::printf("clock sync protocol: %u checks\n", checks);
  return 0;
}
