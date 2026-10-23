#include "support/rtc_rx8130/transport.hpp"
int main() {
  Clock absent;
  expect(absent.read().unavailable, "missing device");
  reset();
  Clock probe; probe.begin(m5::In_I2C);
  trace.clear(); bank[0x1d]=0x98;
  probe.setAndVerify(target);
  unsigned dateWrite=0, dateRead=0, flagWrite=0;
  std::vector<unsigned> flagReads;
  for (unsigned i=0;i<trace.size();++i) {
    const auto& op=trace[i];
    if (op.reg==0x10 && op.write) dateWrite=i+1;
    if (op.reg==0x10 && !op.write && !dateRead) dateRead=i+1;
    if (op.reg==0x1d && op.write) flagWrite=i+1;
    if (op.reg==0x1d && !op.write) flagReads.push_back(i+1);
  }
  for (int scenario=0;scenario<12;++scenario) {
    reset();
    Clock rtc;
    if (scenario==1) m5::In_I2C.setPort(-1,-1,-1);
    rtc.begin(m5::In_I2C); trace.clear();
    bank[0x1d]=0x98;
    if (scenario==2) failAt=dateWrite;
    if (scenario==3) failAt=flagReads[0];
    if (scenario==4) failAt=flagWrite;
    if (scenario==5) failAt=flagReads[1];
    if (scenario==6) failAt=dateRead;
    if (scenario==7) failAt=flagReads[2];
    if (scenario==8) stickyFlags=0x80;
    if (scenario==9) corruptRead=true;
    if (scenario==10) advance=2;
    if (scenario==11) advance=3;
    const Reading r=rtc.setAndVerify(target);
    if (scenario==0 || scenario==10) {
      expect(r.valid && !r.lowVoltage && r.utcSeconds==target+advance,
             "set, clear low flag, tolerate elapsed seconds");
      expect(bank[0x1d]==0x18,"other flag bits retained");
    } else {
      expect(!r.valid,"failed operation never valid");
      if (scenario==1) expect(r.unavailable,"disabled");
      if (scenario>=2 && scenario<=7) expect(r.ioError,"I/O errors distinguished");
      if (scenario==8) expect(r.lowVoltage,"sticky low flag read back");
      if (scenario==9) expect(r.invalidDate,"bad date");
      if (scenario==11) expect(r.verificationFailed,"outside tolerance");
    }
  }
  reset(); Clock rtc; rtc.begin(m5::In_I2C); trace.clear();
  expect(rtc.setAndVerify(-1).invalidDate && trace.empty(),"invalid request makes no writes");
  const uint8_t epoch[]={0,0,0,0x40,1,1,0};
  std::memcpy(bank+0x10,epoch,7); bank[0x1d]=0x80;
  const Reading low=rtc.read();
  expect(!low.valid && low.lowVoltage && !low.ioError && low.utcSeconds==946684800,
         "low voltage retains candidate but not validity");
  failAt=trace.size()+1;
  const Reading io=rtc.read();
  expect(!io.valid && io.ioError,"failed read distinct from no low voltage");
  failAt=0; bank[0x1d]=0;
  expect(rtc.read().valid,"healthy candidate");
  std::printf("RTC migrated adapter checks=%u failures=%u (29 original expectations + burst checks)\n",checks,failures);
  return failures ? 1 : 0;
}
