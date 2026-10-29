#include "support/rtc_rx8130/transport.hpp"
#include "board.hpp"
#include "m5_startup.inc"
#include <new>

uint32_t host_time_ms = 0;
HostSerial Serial;
m5::M5Unified M5;
namespace m5 { unsigned uncheckedReads = 0; }
namespace {
constexpr unsigned kPm1Address = 0x6E, kRtcAddress = 0x32;
constexpr unsigned kHoldCfg = 0x07, kHoldSeed = 0x03, kLdoHold = 0x20;

void bootBegin() {
  M5.~M5Unified(); new (&M5) m5::M5Unified;
  bank[kHoldCfg] = kHoldSeed;
  c152::begin("startup regression");
  expect(m5::uncheckedReads==0,"no unchecked date decoder during board startup");
  expect(!M5.Rtc.isEnabled(),"framework RTC stays unowned");
  expect(!M5.lastConfig.internal_rtc && !M5.lastConfig.external_rtc
         && !M5.lastConfig.disable_rtc_irq,"all automatic RTC paths disabled");
  expect(M5.Imu.isEnabled(),"IMU still initialized");
  expect(M5.lastConfig.internal_spk && M5.lastConfig.output_power,
         "audio and output power configuration preserved");
}
void bootBoard() {
  bootBegin();
  expect(trace.size()==3,"board sets the PM1 LDO hold with one read-modify-write");
  for (const auto& op:trace) {
    expect(op.addr==kPm1Address && op.reg==kHoldCfg && op.len==1,
           "board PM1 traffic stays on HOLD_CFG");
    expect(op.addr!=kRtcAddress,"board startup leaves the RTC untouched");
  }
  if (trace.size()==3) {
    expect(!trace[0].write && trace[0].value==kHoldSeed,
           "board reads the current PM1 hold setting");
    expect(trace[1].write && trace[1].value==(kHoldSeed|kLdoHold),
           "board keeps other hold bits and sets the LDO bit");
    expect(!trace[2].write && trace[2].value==(kHoldSeed|kLdoHold),
           "board reads the PM1 hold setting back");
  }
  expect(busBegins==0 && dateWrites==0,"board leaves RTC and shared bus untouched");
  trace.clear();
}
void bootWithPm1Failure(unsigned failing) {
  reset(); failAt=failing;
  bootBegin();
  expect(trace.size()==failing,"PM1 hold failure stops after the failing access");
  bool writeSeen=false;
  for (const auto& op:trace) {
    expect(op.addr==kPm1Address && op.reg==kHoldCfg && op.len==1,
           "failed PM1 hold traffic stays on HOLD_CFG");
    if (op.write) {
      writeSeen=true;
      expect(op.value==(kHoldSeed|kLdoHold),"the PM1 hold write keeps other bits");
    }
  }
  expect(writeSeen==(failing>1),"PM1 hold is written at most once");
  if (failing==2) expect(!(bank[kHoldCfg]&kLdoHold),
                         "failed PM1 hold write leaves the register unchanged");
  trace.clear(); failAt=0;
  Clock rtc; rtc.begin(M5.In_I2C);
  expect(rtc.read().valid,"PM1 hold failure leaves RX8130 startup intact");
  for (const auto& op:trace) expect(op.addr!=kPm1Address,"no PM1 retry after startup");
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
      const auto r=rtc.read();
      expect(bank[0x1d]&bit,"new startup flag survives strict read");
      if (bit==2) expect(!r.valid,"startup VLF invalidates the clock");
      else expect(r.valid && r.lowVoltage,"startup VBLF only warns");
      expect(dateWrites==0 && busBegins==0 && delays==0,"startup has no date writes/bus begin/delay");
      for(const auto& op:trace) expect(!op.write || (op.reg!=0x1d && op.reg!=0x1c),
                                     "startup never clears Flag or runs full recovery");
    }
    bootWithPm1Failure(1);
    bootWithPm1Failure(2);
    bootWithPm1Failure(3);
  }
  std::printf("board startup checks=%u failures=%u unchecked=%u\n",checks,failures,m5::uncheckedReads);
  return failures ? 1 : 0;
}
