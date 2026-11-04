#include "support/rtc_rx8130/transport.hpp"

namespace {
void clearTrace() { trace.clear(); snapshots.clear(); dateReads=0; }
void fault(unsigned kind) {
  if (kind==1) bank[0x1e]=0x40;
  if (kind==2) bank[0x1e]=0x80;
  if (kind==3) bank[0x31]=0xff;
  if (kind==4) bank[0x20]=0xa5;
  if (kind==5) bank[0x1d]=0x82;
}
void lifecycle() {
  reset();
  Clock cold;
  expect(cold.read().unavailable && cold.setAndVerify(target).unavailable,
         "clock without a bus is unavailable");
  m5::I2C_Class bus;
  cold.begin(bus);
  expect(cold.read().unavailable,"disabled bus is unavailable");
  bus.setPort(0,1,2);
  expect(cold.setAndVerify(target).valid,"explicit set recovers after bus becomes available");
  for (unsigned value : {0u, 0x81u, 0xa5u, 0xffu}) {
    reset(); bank[0x20]=value;
    Clock rtc; rtc.begin(m5::In_I2C);
    const Reading r=rtc.read();
    expect(value==0xa5 ? !r.valid && r.verificationFailed : r.valid && r.utcSeconds==target,
           "unfinished calibration is rejected; other old user RAM does not lose the date");
    Clock reboot; reboot.begin(m5::In_I2C);
    const Reading next=reboot.read();
    expect(next.valid==(value!=0xa5) && next.utcSeconds==target,
           "restart preserves UTC and unfinished calibration status");
  }
}
void startupMatrix() {
  for(unsigned kind=0;kind<=5;++kind) {
    reset(); fault(kind);
    std::array<uint8_t,256> initial; std::memcpy(initial.data(),bank,256);
    Clock probe; probe.begin(m5::In_I2C);
    const auto good=trace; const auto states=snapshots;
    expect(!good.empty(),"startup fault matrix has I/O boundaries");
    if(kind>=1 && kind<=3) {
      for(unsigned boundary=0;boundary<=states.size();++boundary) {
        reset(); const auto& saved=boundary?states[boundary-1]:initial;
        std::memcpy(bank,saved.data(),256);
        Clock reboot; reboot.begin(m5::In_I2C);
        expect(!reboot.read().valid,"interrupted STOP/TEST initialization cannot make a date trusted");
      }
    }
    for(unsigned i=1;i<=good.size();++i) {
      reset(); fault(kind); failAt=i;
      Clock rtc; rtc.begin(m5::In_I2C);
      Reading r=rtc.read();
      expect(!r.valid && r.ioError,"startup I/O failure cannot expose trusted time");
      failAt=0; clearTrace();
      expect(rtc.setAndVerify(target).valid,"explicit calibration recovers startup I/O failure");
    }
    for(unsigned i=1;i<good.size();++i) {
      if(good[i].write || !good[i-1].write || good[i].reg!=good[i-1].reg) continue;
      reset(); fault(kind); mismatchAt=i+1;
      mismatchBits=good[i].reg==0x1e?0x40:(good[i].reg==0x31?2:1);
      Clock rtc; rtc.begin(m5::In_I2C);
      Reading r=rtc.read();
      expect(!r.valid && r.verificationFailed && !r.ioError,"startup readback mismatch cannot expose trusted time");
      mismatchAt=0; clearTrace();
      expect(rtc.setAndVerify(target).valid,"explicit calibration recovers readback mismatch");
    }
  }
}
void readMatrix() {
  reset(); Clock rtc; rtc.begin(m5::In_I2C); bank[0x31]=0x80;
  expect(rtc.read().verificationFailed,"runtime extension TEST invalidates reading");
  expect(rtc.setAndVerify(target).valid,"explicit calibration recovers extension TEST");
}
void setMatrix() {
  for(unsigned kind=0;kind<=5;++kind) {
    reset(); fault(kind); Clock probe; probe.begin(m5::In_I2C); clearTrace();
    expect(probe.setAndVerify(target).valid,"calibration baseline succeeds");
    const auto good=trace; const auto states=snapshots;
    unsigned dateOp=0, cleanOp=0;
    for(unsigned i=0;i<good.size();++i) {
      if(good[i].write && good[i].reg==0x10) dateOp=i+1;
      if(good[i].write && good[i].reg==0x20 && good[i].value==0) cleanOp=i+1;
    }
    expect(dateOp && cleanOp,"calibration interruption matrix has date and completion boundaries");
    for(unsigned i=0;i<states.size();++i) {
      reset(); std::memcpy(bank,states[i].data(),256);
      const bool dirty=bank[0x20]==0xa5;
      Clock reboot; reboot.begin(m5::In_I2C);
      const Reading r=reboot.read();
      if(dirty) expect(!r.valid,"interrupted calibration cannot become trusted after restart");
      if(i+1>=cleanOp) expect(r.valid,"completed calibration survives restart");
    }
    for(unsigned n=0;n<=7;++n) {
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
      const uint8_t oldDate[]={0x11,0x11,0x11,0x20,0x11,0x11,0x11};
      std::memcpy(bank+0x10,oldDate,7);
      failAt=dateOp; partialBytes=n;
      const Reading r=rtc.setAndVerify(target);
      expect(!r.valid && r.ioError,"partial date write cannot report success");
      failAt=0;
      expect(!rtc.read().valid,"failed date write cannot unlock current process");
      Clock reboot; reboot.begin(m5::In_I2C);
      expect(!reboot.read().valid,"failed date write cannot unlock restarted process");
    }
    for(unsigned i=1;i<=good.size();++i) {
      if(!good[i-1].write || good[i-1].reg!=0x20) continue;
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
      failAt=i; applyFailedWrite=true;
      Reading r=rtc.setAndVerify(target);
      expect(!r.valid && r.ioError,"uncertain transaction marker cannot report success");
      failAt=0;
      expect(!rtc.read().valid,"uncertain marker cannot unlock current process");
      reset(); fault(kind); Clock mismatch; mismatch.begin(m5::In_I2C); clearTrace();
      mismatchAt=i+1; mismatchBits=1;
      r=mismatch.setAndVerify(target);
      expect(!r.valid && r.verificationFailed && !r.ioError,"marker mismatch cannot report success");
    }
    for(unsigned failed : {cleanOp,cleanOp+1,unsigned(good.size())}) {
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
      failAt=failed; failAt2=failed+1;
      const Reading r=rtc.setAndVerify(target);
      expect(!r.valid && r.ioError,"failed dirty repair cannot report success");
      failAt=failAt2=0;
      expect(!rtc.read().valid,"failed dirty repair cannot unlock current process");
    }
  }
}
void finalConfirmation() {
  for(int seconds:{-1,0,1,2,3}) {
    reset(); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
    advance=seconds; advanceFromRead=2;
    const Reading r=rtc.setAndVerify(target);
    expect(seconds>=0 && seconds<=2 ? r.valid && r.utcSeconds==target+seconds
           : !r.valid && r.verificationFailed,
           "date changing at final confirmation must still satisfy 0..2 second tolerance");
    if(seconds<0 || seconds>2) {
      // Keep the hardware state but remove the transient time shift before reboot.
      advance=0;
      Clock reboot; reboot.begin(m5::In_I2C);
      const Reading next=reboot.read();
      expect(!next.valid && next.verificationFailed,
             "failed final confirmation cannot become trusted after restart");
    }
  }
}
}
int main() {
  lifecycle(); startupMatrix(); readMatrix(); setMatrix(); finalConfirmation();
  std::printf("RTC lifecycle checks=%u failures=%u (host bank rebuild, not physical backup)\n",checks,failures);
  return failures?1:0;
}
