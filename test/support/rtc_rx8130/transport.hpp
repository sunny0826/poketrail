#pragma once
#include <M5Unified.h>
#include "device_clock_hw.hpp"
#include "calendar.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <initializer_list>
#include <array>
#include <memory>

namespace {
using pokedex::device_clock::Reading;
using pokedex::device_clock::Clock;
const int64_t target = 1792670400LL;
struct Op { bool write; unsigned addr, reg, len, value; };
uint8_t bank[256];
std::vector<Op> trace;
std::vector<std::array<uint8_t,256>> snapshots;
unsigned delays = 0, delayMs = 0, busBegins = 0;
unsigned mismatchAt = 0, mismatchBits = 1, failAt2 = 0, injectionHits = 0;
bool applyFailedWrite = false;
void snapshot() {
  std::array<uint8_t,256> copy;
  std::memcpy(copy.data(),bank,256);
  snapshots.push_back(copy);
}
unsigned failAt, injectAt, injectBits, stickyFlags, partialBytes;
bool stickStop, corruptRead, changeAfterRead;
int advance;
unsigned advanceFromRead = 0;
unsigned failures = 0, checks = 0, dateReads = 0, dateWrites = 0;
void expect(bool ok, const char* message) {
  ++checks;
  if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
void reset() {
  std::memset(bank, 0, sizeof bank);
  const uint8_t date[] = {0, 0, 0x12, 0x10, 0x22, 0x10, 0x26};
  std::memcpy(bank+0x10, date, 7);
  bank[0x1f] = 0x30;
  m5::In_I2C.setPort(0,1,2);
  snapshots.clear(); delays = delayMs = busBegins = 0;
  mismatchAt = failAt2 = injectionHits = 0; mismatchBits = 1; applyFailedWrite = false;
  trace.clear(); failAt = injectAt = injectBits = stickyFlags = partialBytes = 0;
  stickStop = corruptRead = changeAfterRead = false;
  advance = 0; advanceFromRead = 0; dateReads = dateWrites = 0;
}
bool operation(bool write, unsigned addr, unsigned reg, unsigned len) {
  trace.push_back({write,addr,reg,len,0});
  if (trace.size() == injectAt) { bank[0x1d] |= injectBits; ++injectionHits; }
  return trace.size() != failAt && trace.size() != failAt2;
}
uint8_t bcd(int n) { return uint8_t((n/10)*16+n%10); }
void shiftDate() {
  pokedex::calendar::CivilTime v;
  pokedex::calendar::fromUtc(target+advance,0,v);
  uint8_t weekday=0; pokedex::calendar::weekday(v,weekday);
  uint8_t date[]={bcd(v.second),bcd(v.minute),bcd(v.hour),uint8_t(1<<weekday),
                  bcd(v.day),bcd(v.month),bcd(v.year%100)};
  std::memcpy(bank+0x10,date,7);
}
void writeByte(unsigned reg, uint8_t value) {
  if (reg == 0x1d) {
    bank[reg] = uint8_t((bank[reg] & value & 0xBE) | (bank[reg] & 1) | stickyFlags);
  } else if (reg == 0x1e) {
    bank[reg] = value | (stickStop ? 0x40 : 0);
  } else { bank[reg] = value; }
}
}
namespace m5 {
I2C_Class In_I2C;
I2C_Class Ex_I2C;
void I2C_Class::setPort(int port, int sda, int scl) {
  _port_num=port; _pin_sda=sda; _pin_scl=scl;
}
bool I2C_Class::begin() { ++busBegins; return true; }
bool I2C_Class::readRegister(uint8_t a, uint8_t reg, uint8_t* out,
                             size_t len, uint32_t) const {
  if (!operation(false,a,reg,len)) { snapshot(); return false; }
  if (reg==0x10) {
    ++dateReads;
    expect(len==7,"date read is one complete burst");
    if (advance && dateReads>=advanceFromRead) shiftDate();
    if (corruptRead) bank[0x15]=0x13;
  }
  std::memcpy(out,bank+reg,len);
  if (trace.size()==mismatchAt) out[0] ^= mismatchBits;
  trace.back().value=out[0];
  if (reg==0x10 && changeAfterRead) bank[0x10]=0x1a;
  snapshot();
  return true;
}
bool I2C_Class::writeRegister(uint8_t a, uint8_t reg, const uint8_t* data,
                              size_t len, uint32_t) const {
  const bool ok=operation(true,a,reg,len);
  trace.back().value=data[0];
  if (reg==0x10) ++dateWrites;
  if (!ok) {
    if (reg==0x10) for (unsigned i=0;i<partialBytes && i<len;++i) writeByte(reg+i,data[i]);
    if (applyFailedWrite) for(size_t i=0;i<len;++i) writeByte(reg+i,data[i]);
    snapshot();
    return false;
  }
  for (size_t i=0;i<len;++i) writeByte(reg+i,data[i]);
  snapshot();
  return true;
}
bool I2C_Class::writeRegister8(uint8_t a,uint8_t r,uint8_t v,uint32_t f) const {
  return writeRegister(a,r,&v,1,f);
}
uint8_t I2C_Class::readRegister8(uint8_t a,uint8_t r,uint32_t f) const {
  uint8_t v=0; readRegister(a,r,&v,1,f); return v;
}
bool I2C_Class::bitOn(uint8_t a,uint8_t r,uint8_t b,uint32_t f) const {
  return writeRegister8(a,r,readRegister8(a,r,f)|b,f);
}
bool I2C_Class::bitOff(uint8_t a,uint8_t r,uint8_t b,uint32_t f) const {
  return writeRegister8(a,r,readRegister8(a,r,f)&~b,f);
}
}

void rtcTestDelay(unsigned ms) { ++delays; delayMs += ms; }
