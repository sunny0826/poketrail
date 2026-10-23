#include "support/rtc_rx8130/transport.hpp"
int main() {
  std::unique_ptr<Clock> rtc;
  const auto restart = [&]() {
    reset(); rtc.reset(new Clock); rtc->begin(m5::In_I2C);
    trace.clear(); snapshots.clear();
  };
  Clock absent, disabledRtc;
  m5::I2C_Class noBus;
  disabledRtc.begin(noBus);
  expect(absent.read().unavailable && absent.setAndVerify(target).unavailable,"missing RTC");
  expect(disabledRtc.read().unavailable && disabledRtc.setAndVerify(target).unavailable,"disabled RTC");
  restart();
  expect(rtc->setAndVerify(-1).invalidDate && trace.empty(),"invalid request has no I/O");
  expect(rtc->setAndVerify(4102444800LL).invalidDate && trace.empty(),"upper UTC limit has no I/O");
  expect(rtc->read().valid && dateReads==1,"healthy date read once");
  restart(); changeAfterRead=true;
  expect(rtc->read().valid && dateReads==1,"decodes only captured burst, not changed registers");
  for (unsigned bit : {2u,128u}) {
    restart(); bank[0x1d]=bit;
    Reading r=rtc->read();
    expect(!r.valid && !r.ioError,"preexisting voltage/oscillation invalidity");
    expect(r.lowVoltage==(bit==128),"VBLF distinct from VLF");
    if (bit==2) expect(dateReads==0,"VLF blocks unchecked date access");
    for (unsigned op : {2u,3u}) {
      restart(); injectAt=op; injectBits=bit;
      expect(!rtc->read().valid,"flag during or after burst invalidates candidate");
    }
  }
  for (unsigned control : {0x40u,0x80u,0xc0u}) {
    restart(); bank[0x1e]=control;
    expect(!rtc->read().valid,"STOP/TEST are not a running clock");
    expect(rtc->setAndVerify(target).valid && bank[0x1e]==0,"explicit set restores STOP/TEST");
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
    expect(rtc->read().valid,"all seven single weekday bits accepted");
  }
  restart(); bank[0x1d]=0x80;
  Reading low=rtc->read();
  expect(!low.valid && low.lowVoltage && low.utcSeconds==target,"low battery retains safe candidate");
  for (unsigned flags : {0u,0x80u,2u,0x82u,0xbfu}) {
    restart(); bank[0x1d]=flags;
    Reading r=rtc->setAndVerify(target);
    expect(r.valid && r.utcSeconds==target && !r.lowVoltage,"explicit calibration succeeds");
    expect(bank[0x1d]==(flags&0x3d),"other flags including read-only VBFF preserved");
    expect(dateWrites==1 && dateReads==2,"single date write and two validated readbacks");
    const std::vector<Op> successful=trace;
    for (unsigned i=1;i<=successful.size();++i) {
      restart(); bank[0x1d]=flags; failAt=i;
      r=rtc->setAndVerify(target);
      expect(!r.valid && r.ioError,"every calibration I/O stage fails closed");
    }
  }
  restart(); rtc->read();
  const unsigned readOps=trace.size();
  for (unsigned i=1;i<=readOps;++i) {
    restart(); failAt=i;
    Reading r=rtc->read();
    expect(!r.valid && r.ioError,"every normal read I/O stage fails closed");
  }
  for (unsigned bit : {2u,128u}) {
    restart(); bank[0x1d]=bit; stickyFlags=bit;
    Reading r=rtc->setAndVerify(target);
    expect(!r.valid && !r.ioError && dateWrites==0,"unclearable flag refuses time write");
    expect(r.lowVoltage==(bit==128),"unclearable VLF and VBLF classified separately");
  }
  restart(); bank[0x1d]=2; stickStop=true;
  expect(!rtc->setAndVerify(target).valid && dateWrites==0,"STOP stuck after recovery refuses time write");
  for (unsigned initial : {0u,0x80u,2u,0x82u}) {
    restart(); bank[0x1d]=initial;
    rtc->setAndVerify(target);
    const std::vector<Op> successful=trace;
    unsigned dateOp=0, clearOp=0;
    for (unsigned i=0;i<successful.size();++i) {
      if(successful[i].write && successful[i].reg==0x10) dateOp=i+1;
      if(successful[i].write && successful[i].reg==0x1d) clearOp=i+1;
    }
    for(unsigned i=dateOp;i<=successful.size();++i) for(unsigned bit:{2u,128u}) {
      restart(); bank[0x1d]=initial; injectAt=i; injectBits=bit;
      Reading r=rtc->setAndVerify(target);
      expect(!r.valid && (bank[0x1d]&bit),"new flag during write/readback retained and invalid");
    }
    if(clearOp) {
      restart(); bank[0x1d]=initial; injectAt=clearOp; injectBits=0x3d;
      expect(rtc->setAndVerify(target).valid && (bank[0x1d]&0x3d)==0x3d,
             "other flags arriving at clear are not lost");
      if(!(initial&2)) {
        restart(); bank[0x1d]=initial; injectAt=clearOp; injectBits=2;
        expect(!rtc->setAndVerify(target).valid && (bank[0x1d]&2),
               "VBLF-only clear cannot erase a new VLF");
      }
    }
    for(unsigned n=0;n<=7;++n) {
      restart(); bank[0x1d]=initial; failAt=dateOp; partialBytes=n;
      Reading r=rtc->setAndVerify(target);
      expect(!r.valid && r.ioError,"partial or completed date write with NACK stays invalid");
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
  restart(); bank[0x1d]=2; bank[0x1f]=0xe7; bank[0x31]=0xff;
  r=rtc->setAndVerify(target);
  expect(r.valid && bank[0x1f]==0xf7 && bank[0x31]==1,
         "recovery preserves charge settings, sets INIEN, clears reserved TEST bits");
  std::printf("RTC register checks=%u failures=%u (real RX8130 + I2C substitute)\n",checks,failures);
  return failures ? 1 : 0;
}
