#include "support/rtc_rx8130/transport.hpp"
#include <type_traits>

static_assert(!std::is_copy_constructible<Clock>::value, "one owner");
static_assert(!std::is_move_constructible<Clock>::value, "stable owner");
static_assert(!std::is_copy_assignable<Clock>::value && !std::is_move_assignable<Clock>::value, "no owner transfer");
static_assert(!std::is_convertible<Clock*,m5::RTC_Base*>::value, "no exposed RTC");
namespace {
unsigned writes(unsigned reg) {
  unsigned count=0;
  for (const auto& op:trace) if(op.write && op.reg==reg) ++count;
  return count;
}
void clearTrace() { trace.clear(); snapshots.clear(); dateReads=dateWrites=0; }
void fault(unsigned kind) {
  if (kind==1) bank[0x1e]=0x40;
  if (kind==2) bank[0x1e]=0x80;
  if (kind==3) bank[0x31]=0xff;
  if (kind==4) bank[0x20]=0xa5;
  if (kind==5) bank[0x1d]=0x82;
}
void expectPrefix(const std::vector<Op>& good,unsigned n) {
  expect(trace.size()>=n,"fault position was reached");
  for(unsigned i=0;i<n && i<trace.size();++i)
    expect(trace[i].reg==good[i].reg && trace[i].write==good[i].write
           && trace[i].len==good[i].len,"fault trace matches actual successful prefix");
}
void dumpTrace(const char* name) {
  std::printf("TRACE %s (%zu I/O)\n",name,trace.size());
  for(unsigned i=0;i<trace.size();++i) {
    const auto& op=trace[i];
    std::printf("%u %c %02x len=%u value=%02x\n",i+1,op.write?'W':'R',op.reg,op.len,op.value);
  }
}
void lifecycle() {
  reset();
  Clock cold;
  expect(trace.empty() && busBegins==0,"construction has no I/O");
  expect(cold.read().unavailable && cold.setAndVerify(target).unavailable && trace.empty(),
         "Cold does not discover a bus");
  m5::I2C_Class bus;
  cold.begin(bus);
  expect(cold.read().unavailable && trace.empty(),"disabled bus has no I/O");
  bus.setPort(0,1,2);
  cold.begin(bus);
  expect(cold.read().unavailable && trace.empty(),"repeated begin never retries NoBus");
  expect(cold.setAndVerify(target).valid && busBegins==0,"explicit set retries enabled NoBus");
  for (unsigned value=0;value<256;++value) {
    reset(); bank[0x20]=value; bank[0x1f]=value; bank[0x31]=value&1;
    bank[0x1c]=0x6d; bank[0x21]=0x12; bank[0x22]=0x34; bank[0x23]=0x56;
    Clock rtc; rtc.begin(m5::In_I2C);
    expect(bank[0x1f]==((value&0xf7)|0x30),"all power/threshold settings preserved");
    expect(bank[0x1c]==0x6d && bank[0x31]==(value&1),"healthy extensions untouched");
    expect(bank[0x21]==0x12 && bank[0x22]==0x34 && bank[0x23]==0x56,"other user RAM untouched");
    expect(writes(0x20)==0 && dateWrites==0 && writes(0x1d)==0 && delays==0 && busBegins==0,
           "healthy/dirty marker startup has no unrelated side effects");
    const Reading r=rtc.read();
    expect(value==0xa5 ? !r.valid && r.verificationFailed : r.valid && r.utcSeconds==target,
           "only A5 is dirty, old RAM does not force calibration");
    clearTrace(); rtc.begin(m5::Ex_I2C);
    expect(trace.empty(),"repeated begin ignores different bus");
    rtc.read(); rtc.read();
    for(const auto& op:trace) expect(!op.write,"ordinary reads never write");
    expect(delays==0,"ordinary reads never delay");
    Clock reboot; reboot.begin(m5::In_I2C);
    const Reading next=reboot.read();
    expect(next.valid==(value!=0xa5) && next.utcSeconds==target && dateWrites==0,
           "second owner preserves original UTC and dirty status without date write");
  }
}
void startupMatrix() {
  for(unsigned kind=0;kind<=5;++kind) {
    reset(); fault(kind);
    std::array<uint8_t,256> initial; std::memcpy(initial.data(),bank,256);
    Clock probe; probe.begin(m5::In_I2C);
    const auto good=trace; const auto states=snapshots;
    dumpTrace(kind==0?"begin healthy":"begin fault");
    expect(good.size()==states.size(),"every I/O has an after-bank snapshot");
    expect(delays==0 && dateWrites==0 && writes(0x1d)==0 && writes(0x1c)==0,
           "begin is not explicit recovery");
    if(kind>=1 && kind<=3) {
      expect(bank[0x20]==0xa5 && bank[0x1e]==0 && !(bank[0x31]&0xfe),
             "persistent evidence precedes cleared STOP/TEST");
      for(unsigned boundary=0;boundary<=states.size();++boundary) {
        reset(); const auto& saved=boundary?states[boundary-1]:initial;
        std::memcpy(bank,saved.data(),256);
        Clock reboot; reboot.begin(m5::In_I2C);
        expect(!reboot.read().valid && bank[0x20]==0xa5,
               "new owner at every before/after I/O boundary retains fault");
      }
    }
    for(unsigned i=1;i<=good.size();++i) {
      reset(); fault(kind); failAt=i;
      Clock rtc; rtc.begin(m5::In_I2C); expectPrefix(good,i);
      Reading r=rtc.read();
      expect(!r.valid && r.ioError && !r.verificationFailed,"each begin NACK is InitIoError");
      const size_t stopped=trace.size(); rtc.begin(m5::In_I2C); rtc.read();
      expect(trace.size()==stopped,"failed begin and ordinary read do not retry");
      if(i<=4) expect(writes(0x1e)==0 && writes(0x1f)==0 && writes(0x20)==0,
                     "all pre-state reads succeed before any write");
      failAt=0; clearTrace();
      expect(rtc.setAndVerify(target).valid,"explicit initialization retry can succeed");
      expect(writes(0x30)<=2 && dateWrites==1 && delays<=1,"retry bounded to one init and recovery");
      // A second failure ends the explicit retry, with no polling loop.
      reset(); fault(kind); failAt=i;
      Clock twice; twice.begin(m5::In_I2C); clearTrace(); failAt=1;
      r=twice.setAndVerify(target);
      expect(!r.valid && r.ioError && trace.size()==1,"retry failing first read ends immediately");
    }
    for(unsigned i=1;i<good.size();++i) {
      if(good[i].write || !good[i-1].write || good[i].reg!=good[i-1].reg) continue;
      reset(); fault(kind); mismatchAt=i+1;
      mismatchBits=good[i].reg==0x1e?0x40:(good[i].reg==0x31?2:1);
      Clock rtc; rtc.begin(m5::In_I2C); expectPrefix(good,i+1);
      Reading r=rtc.read();
      expect(!r.valid && r.verificationFailed && !r.ioError,"init readback mismatch distinguished");
      expect(trace.size()==i+1,"readback mismatch stops further initialization writes");
      mismatchAt=0; clearTrace();
      expect(rtc.setAndVerify(target).valid,"explicit retry recovers readback failure");
    }
    for(unsigned i=1;i<=good.size();++i) for(unsigned bit:{2u,128u}) {
      reset(); fault(kind); injectAt=i; injectBits=bit;
      Clock rtc; rtc.begin(m5::In_I2C); expectPrefix(good,i);
      const Reading r=rtc.read();
      expect(injectionHits==1 && (bank[0x1d]&bit),
             "every startup I/O preserves injected VLF/VBLF");
      if (bit==2 || kind != 0) expect(!r.valid,"startup fault or VLF stays invalid");
      else expect(r.valid && r.lowVoltage,"injected VBLF stays a visible warning");
    }
  }
}
void readMatrix() {
  reset(); Clock probe; probe.begin(m5::In_I2C); clearTrace(); probe.read();
  const auto good=trace;
  for(unsigned i=1;i<=good.size();++i) for(unsigned bit:{2u,128u}) {
    reset(); Clock rtc; rtc.begin(m5::In_I2C); clearTrace(); injectAt=i; injectBits=bit;
    const Reading r=rtc.read();
    expectPrefix(good,i);
    expect(injectionHits==1 && (bank[0x1d]&bit),"flag at every ordinary read I/O visible");
    if (bit==2) expect(!r.valid,"injected VLF at read I/O invalidates");
    else expect(r.valid && r.lowVoltage,"injected VBLF at read I/O keeps valid data");
    expect(delays==0 && writes(0x20)==0,"read faults do not recover or mark");
  }
  reset(); Clock rtc; rtc.begin(m5::In_I2C); bank[0x31]=0x80;
  expect(rtc.read().verificationFailed,"runtime extension TEST invalidates reading");
  clearTrace();
  expect(rtc.setAndVerify(target).valid && delays==1 && delayMs==40,"extension TEST explicitly recovered once");
}
void setMatrix() {
  for(unsigned kind=0;kind<=5;++kind) {
    reset(); fault(kind); Clock probe; probe.begin(m5::In_I2C); clearTrace();
    expect(probe.setAndVerify(target).valid,"calibration baseline succeeds");
    const auto good=trace; const auto states=snapshots;
    dumpTrace(kind==0?"set healthy":"set fault");
    expect(delays==(kind?1u:0u) && delayMs==(kind?40u:0u),"only prior fault takes one 40ms recovery");
    unsigned dateOp=0, cleanOp=0;
    for(unsigned i=0;i<good.size();++i) {
      if(good[i].write && good[i].reg==0x10) dateOp=i+1;
      if(good[i].write && good[i].reg==0x20 && good[i].value==0) cleanOp=i+1;
    }
    expect(dateOp && cleanOp>dateOp,"single date write precedes clean marker");
    // Simulated resets at every boundary with the RTC bank preserved.
    for(unsigned i=0;i<states.size();++i) {
      reset(); std::memcpy(bank,states[i].data(),256);
      const bool dirty=bank[0x20]==0xa5;
      Clock reboot; reboot.begin(m5::In_I2C);
      const Reading r=reboot.read();
      if(dirty) expect(!r.valid,"interrupted dirty transaction cannot become valid after owner rebuild");
      if(i+1>=cleanOp) expect(r.valid,"documented clean/final-read reset window retains verified date");
    }
    for(unsigned n=0;n<=7;++n) {
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
      const uint8_t oldDate[]={0x11,0x11,0x11,0x20,0x11,0x11,0x11};
      const uint8_t newDate[]={0,0,0x12,0x10,0x22,0x10,0x26};
      std::memcpy(bank+0x10,oldDate,7);
      failAt=dateOp; partialBytes=n;
      const Reading r=rtc.setAndVerify(target); expectPrefix(good,dateOp);
      expect(!r.valid && r.ioError && bank[0x20]==0xa5,"all 0..7 byte date write failures stay dirty");
      for(unsigned byte=0;byte<7;++byte)
        expect(bank[0x10+byte]==(byte<n?newDate[byte]:oldDate[byte]),"partial injection applied exactly requested date bytes");
      failAt=0;
      expect(!rtc.read().valid,"failed date write never unlocks current owner");
      Clock reboot; reboot.begin(m5::In_I2C);
      expect(!reboot.read().valid,"failed date write never unlocks next owner");
    }
    for(unsigned i=1;i<=good.size();++i) {
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace(); failAt=i;
      Reading r=rtc.setAndVerify(target); expectPrefix(good,i);
      expect(!r.valid && r.ioError,"each calibration I/O fails closed");
      if(i>=cleanOp) expect(writes(0x20)==3 && bank[0x20]==0xa5,
                            "clean/final failure attempts exactly one dirty repair");
      if(i>=3) { failAt=0; expect(!rtc.read().valid,"dirty latch blocks polling after failure"); }
    }
    for(unsigned i=1;i<=good.size();++i) {
      if(!good[i-1].write || good[i-1].reg!=0x20) continue;
      // A marker write may physically complete while reporting failure.
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
      failAt=i; applyFailedWrite=true;
      Reading r=rtc.setAndVerify(target);
      expect(!r.valid && r.ioError && bank[0x20]==0xa5,"uncertain marker write remains dirty");
      if(i<dateOp) expect(dateWrites==0,"dirty marker failure prevents time write");
      // Successful transmission but mismatching marker readback.
      reset(); fault(kind); Clock mismatch; mismatch.begin(m5::In_I2C); clearTrace();
      mismatchAt=i+1; mismatchBits=1;
      r=mismatch.setAndVerify(target);
      expect(!r.valid && r.verificationFailed && !r.ioError,"marker mismatch distinct from NACK");
      if(i<dateOp) expect(dateWrites==0 && delays==0,"dirty readback failure precedes recovery");
      else expect(bank[0x20]==0xa5 && writes(0x20)==3,"clean mismatch repairs once");
    }
    // Failure of best-effort dirty repair cannot change the failed outcome.
    for(unsigned failed : {cleanOp,cleanOp+1,unsigned(good.size())}) {
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
      failAt=failed; failAt2=failed+1;
      const Reading r=rtc.setAndVerify(target);
      expect(!r.valid && r.ioError && trace.size()==failed+1,"dirty repair NACK is bounded and never success");
      failAt=failAt2=0;
      expect(!rtc.read().valid,"RAM latch survives failed repair");
    }
    // Every post-date transfer, including both marker transfers and last status.
    for(unsigned i=dateOp;i<=good.size();++i) for(unsigned bit:{2u,128u}) {
      reset(); fault(kind); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
      injectAt=i; injectBits=bit;
      const Reading r=rtc.setAndVerify(target); expectPrefix(good,i);
      expect(injectionHits==1 && (bank[0x1d]&bit),"new post-write flags survive all final I/O");
      if (bit==2) {
        expect(!r.valid,"post-write VLF invalidates");
        expect(bank[0x20]==0xa5,"post-write VLF leaves/restores dirty marker");
      } else {
        expect(r.valid && r.lowVoltage,"post-write VBLF only warns");
        expect(bank[0x20]==0,"post-write VBLF still completes the clean marker");
      }
      bool afterDate=false;
      for(const auto& op:trace) {
        if(op.write && op.reg==0x10) afterDate=true;
        if(afterDate) expect(!(op.write && op.reg==0x1d),"never clears Flag after date write");
      }
    }
  }
}
void finalConfirmation() {
  for(int seconds:{-1,0,1,2,3}) {
    reset(); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
    advance=seconds; advanceFromRead=2;
    const Reading r=rtc.setAndVerify(target);
    expect(seconds>=0 && seconds<=2 ? r.valid && r.utcSeconds==target+seconds
           : !r.valid && r.verificationFailed && bank[0x20]==0xa5,
           "second strict confirmation enforces 0..2 seconds independently");
  }
  reset(); Clock probe; probe.begin(m5::In_I2C); clearTrace(); probe.setAndVerify(target);
  const auto good=trace;
  for(unsigned i=2;i<=6;++i) for(unsigned bit:{2u,128u}) {
    reset(); Clock rtc; rtc.begin(m5::In_I2C); clearTrace(); injectAt=i; injectBits=bit;
    const Reading r=rtc.setAndVerify(target); expectPrefix(good,i);
    expect(injectionHits==1 && (bank[0x1d]&bit),
           "new flag after initial snapshot including dirty marker survives");
    if (bit==2) expect(!r.valid,"late VLF still blocks calibration");
    else expect(r.valid && r.lowVoltage,"late VBLF is only a warning");
  }
  for(unsigned power=0;power<256;++power) {
    reset(); Clock rtc; rtc.begin(m5::In_I2C); clearTrace();
    bank[0x1f]=power; bank[0x31]=0xff; bank[0x1d]=0x82;
    expect(rtc.setAndVerify(target).valid,"explicit recovery accepts every prior power setting");
    std::vector<Op> controlWrites;
    for(const auto& op:trace)
      if(op.write && (op.reg==0x1e || op.reg==0x1c || op.reg==0x30 || op.reg==0x31 || op.reg==0x1f))
        controlWrites.push_back(op);
    const unsigned registers[]={0x1e,0x1c,0x30,0x31,0x1f};
    const unsigned values[]={0,4,0,1,(power&0xf7)|0x10};
    expect(controlWrites.size()==5,"exact original five recovery writes");
    for(unsigned i=0;i<controlWrites.size() && i<5;++i)
      expect(controlWrites[i].reg==registers[i] && controlWrites[i].value==values[i],
             "original recovery sequence/bytes and power/VBLFE preservation");
    expect(delays==1 && delayMs==40,"recovery delay bounded to one 40ms call");
  }
  reset(); Clock low; low.begin(m5::In_I2C); clearTrace(); bank[0x1d]=0x80;
  expect(low.setAndVerify(target).valid && delays==0 && writes(0x1c)==0,
         "VBLF alone does not trigger full recovery");
}

}
int main() {
  lifecycle(); startupMatrix(); readMatrix(); setMatrix(); finalConfirmation();
  std::printf("RTC lifecycle checks=%u failures=%u (host bank rebuild, not physical backup)\n",checks,failures);
  return failures?1:0;
}
