"""Exercise the device's actual reply queue with a short-writing USB transport."""
import os
from pathlib import Path
import subprocess
import tempfile

source = Path("firmware/apps/20_pokedex/main.cpp").read_text()


def function(signature):
    start = source.index(signature)
    end = source.index("\n}", start) + 2
    return source[start:end]


prefix = r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
struct FakeSerial {
  std::string bytes;
  size_t limit = 4;
  int availableForWrite() { return 256; }
  size_t write(const uint8_t* p, size_t n) {
    n = std::min(n, limit);
    bytes.append(reinterpret_cast<const char*>(p), n);
    return n;
  }
} Serial;
char clockTx[256];
size_t clockTxLength = 0, clockTxSent = 0;
bool clockReplyPending() { return clockTxSent < clockTxLength; }
"""
checks = r"""
int main() {
  const char* line = "PTC q mode=official rtc=1 ready=1";
  const std::string expected = std::string("\n") + line + "\n";
  queueClockReply(line, std::strlen(line));
  pumpClockTransmit();
  assert(Serial.bytes.size() == 4 && clockReplyPending());
  queueClockReply("replacement", 11);
  Serial.limit = 0;
  pumpClockTransmit();
  assert(Serial.bytes.size() == 4 && clockReplyPending());
  Serial.limit = 4;
  for (int i = 0; i < 100 && clockReplyPending(); ++i) pumpClockTransmit();
  assert(!clockReplyPending() && Serial.bytes == expected);
}
"""
with tempfile.TemporaryDirectory(prefix="poketrail-clock-serial-") as directory:
    path = Path(directory)
    cpp = path / "serial.cpp"
    cpp.write_text(prefix + function("void queueClockReply(") +
                   function("void pumpClockTransmit(") + checks)
    binary = path / "serial"
    subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++11", "-Wall",
                    "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("USB reply queue: short write, zero write, queued reply preservation passed")
