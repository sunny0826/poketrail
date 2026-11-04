#include "support/rtc_rx8130/transport.hpp"
#include "board.hpp"
#include "m5_startup.inc"
#include <new>

uint32_t host_time_ms = 0;
HostSerial Serial;
m5::M5Unified M5;
namespace {
constexpr unsigned kHoldCfg = 0x07, kHoldSeed = 0x03, kLdoHold = 0x20;

void bootBegin() {
  M5.~M5Unified(); new (&M5) m5::M5Unified;
  bank[kHoldCfg] = kHoldSeed;
  c152::begin("startup regression");
  expect(M5.Imu.isEnabled(),"board startup leaves step sensor available");
}
void bootBoard() {
  bootBegin();
  expect(bank[kHoldCfg]==(kHoldSeed|kLdoHold),"power-off keeps RTC supply and preserves other hold settings");
  trace.clear();
}
void bootWithPm1Failure(unsigned failing) {
  reset(); failAt=failing;
  bootBegin();
  trace.clear(); failAt=0;
  Clock rtc; rtc.begin(M5.In_I2C);
  expect(rtc.read().valid,"PM1 hold failure still permits clock startup");
}
}
int main(int argc,char** argv) {
  reset();
  const bool zero=argc>1 && !std::strcmp(argv[1],"weekday-zero");
  if (zero) {
    bank[0x13]=0; bank[0x1d]=6;
    bootBoard(); Clock rtc; rtc.begin(M5.In_I2C);
    expect(!rtc.read().valid,"original WEEK zero and VLF input rejected safely");
    bank[0x1d]=0;
    expect(rtc.read().invalidDate,"WEEK zero alone rejected by strict parser");
  } else {
    bootBoard(); Clock probe; probe.begin(M5.In_I2C);
    const auto successful=trace;
    expect(probe.read().valid,"healthy startup retains date");
    for (unsigned i=1;i<=successful.size();++i) for(unsigned bit:{2u,128u}) {
      reset(); bootBoard(); injectAt=i; injectBits=bit;
      Clock rtc; rtc.begin(M5.In_I2C);
      expect(injectionHits==1,"new flag injected at actual startup I/O");
      const auto r=rtc.read();
      if (bit==2) expect(!r.valid,"startup VLF invalidates the clock");
      else expect(r.valid && r.lowVoltage,"startup VBLF only warns");
    }
    bootWithPm1Failure(1);
    bootWithPm1Failure(2);
    bootWithPm1Failure(3);
  }
  std::printf("board startup checks=%u failures=%u\n",checks,failures);
  return failures ? 1 : 0;
}
