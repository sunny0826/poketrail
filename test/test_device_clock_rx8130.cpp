#include "support/rtc_rx8130/transport.hpp"
int main() {
  std::unique_ptr<Clock> rtc;
  const auto restart = [&]() {
    reset(); rtc.reset(new Clock); rtc->begin(m5::In_I2C);
    trace.clear(); snapshots.clear();
  };
  restart();
  expect(rtc->setAndVerify(-1).invalidDate,"negative UTC rejected");
  expect(rtc->setAndVerify(4102444800LL).invalidDate,"UTC outside supported century rejected");
  restart(); changeAfterRead=true;
  expect(rtc->read().valid,"captured healthy date remains valid when registers change after transfer");
  for (unsigned bit : {2u,128u}) {
    restart(); bank[0x1d]=bit;
    Reading r=rtc->read();
    expect(r.lowVoltage==(bit==128),"VBLF distinct from VLF");
    if (bit==2) expect(!r.valid && !r.ioError,"oscillator stop invalidates the candidate");
    else expect(r.valid && !r.ioError,"low-VBAT warning keeps valid date data");
    for (unsigned op : {2u,3u}) {
      restart(); injectAt=op; injectBits=bit;
      Reading injected=rtc->read();
      if (bit==2) expect(!injected.valid,"VLF during or after burst invalidates candidate");
      else expect(injected.valid && injected.lowVoltage,"late VBLF remains visible without hiding data");
    }
  }
  for (unsigned control : {0x40u,0x80u,0xc0u}) {
    restart(); bank[0x1e]=control;
    expect(!rtc->read().valid,"STOP/TEST are not a running clock");
    expect(rtc->setAndVerify(target).valid,"explicit set restores STOP/TEST clock");
  }
  struct Bad { unsigned reg,value; };
  const Bad bad[]={{0x10,0x1a},{0x10,0x60},{0x10,0x80},{0x11,0x1a},{0x11,0x60},
    {0x11,0x80},{0x12,0x1a},{0x12,0x24},{0x12,0x40},{0x13,0},{0x13,3},{0x13,0x80},
    {0x13,0x90},{0x14,0},{0x14,0x1a},{0x14,0x32},{0x14,0x40},{0x15,0},
    {0x15,0x1a},{0x15,0x13},{0x15,0x20},{0x16,0x1a},{0x16,0xa0},{0x16,0xff}};
  for (const Bad& b : bad) {
    restart(); bank[b.reg]=b.value;
    Reading r=rtc->read();
    expect(!r.valid && r.invalidDate && !r.ioError,"bad BCD/range/reserved/weekday rejected");
  }
  restart(); bank[0x15]=2; bank[0x14]=0x29;
  expect(rtc->read().invalidDate,"2026 February 29 rejected");
  bank[0x16]=0x24;
  expect(rtc->read().valid,"2024 February 29 accepted");
  for (unsigned w=1;w<=64;w<<=1) {
    restart(); bank[0x13]=w;
    expect(rtc->read().valid,"all seven weekday values accepted");
  }
  for (unsigned flags : {0u,0x80u,2u,0x82u,0xbfu}) {
    restart(); bank[0x1d]=flags;
    Reading r=rtc->setAndVerify(target);
    expect(r.valid && r.utcSeconds==target && !r.lowVoltage,"explicit calibration succeeds");
    const std::vector<Op> successful=trace;
    for (unsigned i=1;i<=successful.size();++i) {
      restart(); bank[0x1d]=flags; failAt=i;
      r=rtc->setAndVerify(target);
      expect(!r.valid && r.ioError,"every calibration I/O failure refuses success");
    }
  }
  restart(); rtc->read();
  const unsigned readOps=trace.size();
  for (unsigned i=1;i<=readOps;++i) {
    restart(); failAt=i;
    Reading r=rtc->read();
    expect(!r.valid && r.ioError,"every normal read I/O failure refuses trusted time");
  }
  for (unsigned bit : {2u,128u}) {
    restart(); bank[0x1d]=bit; stickyFlags=bit;
    Reading r=rtc->setAndVerify(target);
    if (bit==2) expect(!r.valid && !r.ioError,"unclearable VLF refuses calibration");
    else expect(r.valid && r.lowVoltage,"unclearable VBLF accepts verified date with warning");
  }
  restart(); bank[0x1d]=2; stickStop=true;
  expect(!rtc->setAndVerify(target).valid,"STOP stuck after recovery refuses calibration");
  for (unsigned initial : {0u,0x80u,2u,0x82u}) {
    restart(); bank[0x1d]=initial;
    expect(rtc->setAndVerify(target).valid,"flag injection baseline succeeds");
    const std::vector<Op> successful=trace;
    unsigned dateOp=0, clearOp=0;
    for (unsigned i=0;i<successful.size();++i) {
      if(successful[i].write && successful[i].reg==0x10) dateOp=i+1;
      if(successful[i].write && successful[i].reg==0x1d) clearOp=i+1;
    }
    expect(dateOp!=0,"date write injection boundary exists");
    for(unsigned i=dateOp;i<=successful.size();++i) for(unsigned bit:{2u,128u}) {
      restart(); bank[0x1d]=initial; injectAt=i; injectBits=bit;
      Reading r=rtc->setAndVerify(target);
      if (bit==2) expect(!r.valid,"new VLF during calibration invalidates the result");
      else expect(r.valid && r.lowVoltage,"new VBLF during calibration stays a warning");
    }
    if(clearOp && !(initial&2)) {
      restart(); bank[0x1d]=initial; injectAt=clearOp; injectBits=2;
      expect(!rtc->setAndVerify(target).valid,"warning clear cannot hide a new oscillator failure");
    }
  }
  for(int seconds : {-1,0,1,2,3}) {
    restart(); advance=seconds;
    Reading r=rtc->setAndVerify(target);
    expect(seconds>=0 && seconds<=2 ? r.valid && r.utcSeconds==target+seconds
                                   : !r.valid && r.verificationFailed,"readback tolerance 0..2 seconds");
  }
  restart(); corruptRead=true;
  Reading r=rtc->setAndVerify(target);
  expect(!r.valid && r.invalidDate,"bad readback date rejected");
  std::printf("RTC register checks=%u failures=%u (real RX8130 + I2C substitute)\n",checks,failures);
  return failures ? 1 : 0;
}
