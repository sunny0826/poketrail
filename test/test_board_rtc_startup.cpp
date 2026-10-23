#include "support/rtc_rx8130/transport.hpp"
#include "board.hpp"
#include "m5_startup.inc"
#include <new>

uint32_t host_time_ms = 0;
HostSerial Serial;
m5::M5Unified M5;
namespace m5 { unsigned uncheckedReads = 0; }
namespace {
void bootBoard() {
  M5.~M5Unified(); new (&M5) m5::M5Unified;
  c152::begin("startup regression");
  expect(m5::uncheckedReads==0,"no unchecked date decoder during board startup");
  expect(!M5.Rtc.isEnabled(),"framework RTC stays unowned");
  expect(!M5.lastConfig.internal_rtc && !M5.lastConfig.external_rtc
         && !M5.lastConfig.disable_rtc_irq,"all automatic RTC paths disabled");
  expect(M5.Imu.isEnabled(),"IMU still initialized");
  expect(M5.lastConfig.internal_spk && M5.lastConfig.output_power,
         "audio and output power configuration preserved");
  expect(trace.empty() && busBegins==0,"board leaves RTC and shared bus untouched");
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
      expect(trace[i-1].reg==successful[i-1].reg
             && trace[i-1].write==successful[i-1].write,"injection trace matches success operation");
      expect((bank[0x1d]&bit) && !rtc.read().valid,"new startup flag survives strict read");
      expect(dateWrites==0 && busBegins==0 && delays==0,"startup has no date writes/bus begin/delay");
      for(const auto& op:trace) expect(!op.write || (op.reg!=0x1d && op.reg!=0x1c),
                                     "startup never clears Flag or runs full recovery");
    }
  }
  std::printf("board startup checks=%u failures=%u unchecked=%u\n",checks,failures,m5::uncheckedReads);
  return failures ? 1 : 0;
}
