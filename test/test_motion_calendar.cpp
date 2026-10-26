#include "../firmware/apps/20_pokedex/motion.hpp"
#include "../firmware/apps/20_pokedex/calendar.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks = 0;
void check(bool ok, const char* message) {
  ++checks;
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
using pokedex::motion::Detector;
using pokedex::motion::StepBatch;
using pokedex::calendar::CivilTime;
struct Feed {
  Detector detector;
  uint32_t now;
  unsigned gaps;
  uint16_t tag = 0;
  std::vector<uint32_t> steps;
  std::vector<uint16_t> tags;
  explicit Feed(uint32_t start = 0) : now(start), gaps(0) {}
  StepBatch sample(float magnitude, int axis = 0) {
    const float x = axis == 0 ? magnitude : (axis == 3 ? magnitude * 0.6f : 0);
    const float y = axis == 1 ? -magnitude : (axis == 3 ? magnitude * 0.8f : 0);
    const float z = axis == 2 ? magnitude : 0;
    StepBatch b = detector.sample(x, y, z, now, tag);
    check(b.count <= 3, "bounded batch");
    for (unsigned i = 0; i < b.count; ++i) {
      check(now - b.events[i].monotonicMs < 4000, "peak time precedes confirmation");
      if (!steps.empty()) check(b.events[i].monotonicMs - steps.back() >= 280,
                                "ordered distinct events, including wrap");
      steps.push_back(b.events[i].monotonicMs);
      tags.push_back(b.events[i].tag);
    }
    gaps += b.qualityGap;
    now += 20;
    return b;
  }
  void rest(unsigned ms, int axis = 0) {
    for (unsigned i = 0; i < ms; i += 20) sample(1, axis);
  }
  void cycle(unsigned period = 500, int axis = 0) {
    for (unsigned i = 0; i < period; i += 20)
      sample(1.0f - 0.30f * std::cos(6.28318530718f * float(i) / period), axis);
  }
};
void motionTests() {
  for (int axis = 0; axis < 4; ++axis) {
    Feed f;
    f.rest(200, axis);
    for (unsigned i = 0; i < 10; ++i) f.cycle(500, axis);
    check(f.steps.size() == 10, "ten steps on each axis / diagonal");
  }
  for (unsigned period : {300u, 800u, 1200u, 1400u}) {
    Feed paced;
    for (int i = 0; i < 6; ++i) paced.cycle(period);
    check(paced.steps.size() == 6, "supported slow/fast cadence");
  }
  Feed stationary;
  stationary.rest(10000);
  check(stationary.steps.empty(), "stationary no steps");
  stationary.cycle();
  stationary.rest(3000);
  check(stationary.steps.empty(), "single pickup/setdown no steps");
  // 口袋/背包佩戴：信号是脚步冲击谐波（~9Hz 衰减振荡），低通滤波必须放行
  // 足够的冲击能量让谷值压过阈值，否则步数大量丢失。
  for (float impact : {0.20f, 0.30f}) {
    Feed pocket;
    for (int s = 0; s < 40; ++s)
      for (unsigned t = 0; t < 500; t += 20)
        pocket.sample(1.0f + impact * cosf(6.2831853f * float(t) / 110.0f) * expf(-float(t) / 70.0f));
    check(pocket.steps.size() >= 39, "pocket impact steps counted at walking cadence");
  }
  Feed first;
  first.cycle(); first.cycle();
  check(first.steps.empty(), "two peaks remain unconfirmed");
  first.cycle();
  check(first.steps.size() == 3, "third peak releases all three");
  check(first.steps.back() - first.steps.front() == 1000, "original candidate times retained");
  first.rest(2000);
  const size_t before = first.steps.size();
  first.cycle(); first.cycle();
  check(first.steps.size() == before, "restart requires fresh cadence");
  first.cycle();
  check(first.steps.size() == before + 3, "restart retains candidates");
  Feed noise;
  uint32_t random = 12345;
  for (int i = 0; i < 2000; ++i) {
    random = random * 1664525u + 1013904223u;
    noise.sample(1.0f + float(int(random % 181) - 90) / 1000);
  }
  check(noise.steps.empty(), "bounded irregular noise no steps");
  Feed irregular;
  for (int i = 0; i < 10; ++i) {
    irregular.cycle(300); irregular.rest(100);
    irregular.cycle(300); irregular.rest(700);
  }
  check(irregular.steps.empty(), "irregular full amplitude pulses no cadence");
  Feed suppressed;
  suppressed.cycle(); suppressed.cycle();
  suppressed.detector.suppressUntil(suppressed.now + 2000);
  suppressed.detector.suppressUntil(suppressed.now + 1000);
  for (int i = 0; i < 4; ++i) suppressed.cycle();
  check(suppressed.steps.empty(), "vibration suppressed, shorter call cannot shorten window");
  for (int i = 0; i < 3; ++i) suppressed.cycle();
  check(suppressed.steps.size() == 3, "fresh confirmation after suppression");
  Feed wrapped(UINT32_MAX - 700);
  for (int i = 0; i < 8; ++i) wrapped.cycle();
  check(wrapped.steps.size() == 8, "uint32 wrap keeps cadence");
  Feed wrapSuppression(UINT32_MAX - 400);
  wrapSuppression.detector.suppressUntil(wrapSuppression.now + 1000);
  wrapSuppression.cycle(); wrapSuppression.cycle();
  check(wrapSuppression.steps.empty(), "suppression crosses wrap");
  for (int i = 0; i < 3; ++i) wrapSuppression.cycle();
  check(wrapSuppression.steps.size() == 3, "suppression ends after wrap");
  const float invalids[] = {std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(), 1e30f, 0};
  for (float invalid : invalids) {
    Feed f;
    f.cycle(); f.cycle();
    check(f.sample(invalid).qualityGap, "invalid sample reports quality gap");
    f.cycle(); f.cycle();
    check(f.steps.empty(), "invalid sample discards old candidates");
    f.cycle();
    check(f.steps.size() == 3, "recovery after invalid sample");
  }
  for (uint32_t gap : {80u, 5000u, UINT32_MAX - 40u}) {
    Feed f;
    f.cycle(); f.cycle();
    f.now += gap;
    check(f.sample(1).qualityGap, "time gap/backward reports quality gap");
    f.cycle(); f.cycle();
    check(f.steps.empty(), "gap clears candidate cadence");
  }
  Feed duplicate;
  duplicate.sample(1);
  duplicate.now -= 20;
  check(duplicate.sample(1).qualityGap, "duplicate timestamp invalidates cadence");
  Feed explicitReset;
  explicitReset.cycle(); explicitReset.cycle();
  explicitReset.detector.reset();
  explicitReset.cycle(); explicitReset.cycle();
  check(explicitReset.steps.empty(), "explicit reset clears candidates");
}
void ownershipTests() {
  Feed switched;
  switched.tag = 1; switched.cycle(); switched.cycle();
  switched.tag = 4; switched.cycle();
  check(switched.tags == std::vector<uint16_t>({1, 1, 4}), "two old peaks keep owner after switch");
  Feed twice;
  for (uint16_t tag : {1, 4, 7}) { twice.tag = tag; twice.cycle(); }
  check(twice.tags == std::vector<uint16_t>({1, 4, 7}), "each deferred peak retains its owner");
  Feed falling;
  for (unsigned i = 0; i < 1500; i += 20) {
    unsigned phase = i % 500;
    falling.tag = phase <= 300 ? 1 : 2;
    falling.sample(1.0f - 0.30f * std::cos(6.28318530718f * float(phase) / 500));
  }
  check(falling.tags == std::vector<uint16_t>({1, 1, 1}), "owner freezes at peak, not falling confirmation");
  falling.tag = 2; falling.cycle();
  check(falling.tags.size() == 4 && falling.tags.back() == 2, "confirmed cadence accepts new owner without reset");
  Feed unassigned;
  unassigned.cycle(); unassigned.tag = 7; unassigned.cycle(); unassigned.cycle();
  check(unassigned.tags == std::vector<uint16_t>({0, 7, 7}), "pre-starter peak remains unassigned");
}
void calendarTests() {
  using namespace pokedex::calendar;
  int64_t utc = -1;
  check(toUtc({2000,1,1,0,0,0}, 0, utc) && utc == 946684800, "Unix epoch base");
  check(toUtc({2099,12,31,23,59,59}, 0, utc) && utc == 4102444799LL,
        "last second 2099");
  check(isLeapYear(2000) && !isLeapYear(2100) && isLeapYear(2024), "Gregorian century");
  check(isValid({2024,2,29,23,59,59}) && !isValid({2023,2,29,0,0,0}), "leap day");
  const CivilTime invalid[] = {{1999,12,31,0,0,0}, {2100,1,1,0,0,0},
    {2024,0,1,0,0,0}, {2024,13,1,0,0,0}, {2024,4,31,0,0,0},
    {2024,1,0,0,0,0}, {2024,1,1,24,0,0}, {2024,1,1,0,60,0},
    {2024,1,1,0,0,60}, {2024,1,1,-1,0,0}};
  for (const CivilTime& c : invalid) {
    utc = 42;
    uint32_t key = 42;
    check(!toUtc(c, 0, utc) && utc == 42, "invalid civil preserves output");
    check(!dayKey(c, key) && key == 42, "invalid key preserves output");
  }
  for (int offset : {-721, 841, std::numeric_limits<int>::max()}) {
    utc = 42;
    check(!toUtc({2026,1,1,0,0,0}, offset, utc) && utc == 42, "invalid offset");
  }
  CivilTime c = {2026,1,1,0,0,0};
  check(!toUtc({2000,1,1,0,0,0}, 840, utc), "lower UTC century boundary");
  check(!toUtc({2099,12,31,23,59,59}, -720, utc), "upper UTC century boundary");
  check(!fromUtc(946684800, -720, c) && c.year == 2026, "lower local boundary unchanged");
  check(!fromUtc(4102444799LL, 840, c), "upper local boundary");
  check(!fromUtc(std::numeric_limits<int64_t>::max(), 0, c), "huge UTC rejected");
  check(!fromUtc(-1, 0, c), "negative UTC rejected");
  check(toUtc({2024,3,1,0,0,0}, 480, utc), "local midnight conversion");
  check(fromUtc(utc, 0, c) && c.month == 2 && c.day == 29 && c.hour == 16,
        "positive offset crosses leap day");
  check(fromUtc(utc, 480, c) && c.month == 3 && c.day == 1 && c.hour == 0,
        "positive offset round trip");
  uint32_t key = 0;
  check(dayKey(c, key) && key == 20240301, "actual calendar day key");
  uint8_t week = 255;
  check(weekday({2000,1,1,0,0,0}, week) && week == 6, "Saturday weekday");
  // Every date in the supported century, including negative offsets and DST-free conversion.
  for (int64_t t = 946771200; t < 4102358400LL; t += 86400) {
    for (int offset : {-720, 0, 480, 840}) {
      int64_t recovered = 0;
      check(fromUtc(t, offset, c) && toUtc(c, offset, recovered) && recovered == t,
            "all supported dates round trip");
    }
  }
  // Confirm after midnight; assign each delayed peak with its original monotonic anchor.
  Feed midnight;
  midnight.cycle(); midnight.cycle(); midnight.cycle();
  check(toUtc({2026,10,22,23,59,59}, 480, utc), "midnight anchor");
  std::vector<uint32_t> keys;
  for (uint32_t at : midnight.steps) {
    check(fromUtc(utc + at / 1000, 480, c) && dayKey(c, key), "map actual event time");
    keys.push_back(key);
  }
  check(keys.size() == 3 && keys[0] == 20261022 && keys[1] == 20261022
        && keys[2] == 20261023, "delayed batch spans two days");
}
}
int main() {
  motionTests();
  ownershipTests();
  calendarTests();
  std::printf("PASS motion/calendar: %u checks (synthetic signals only)\n", checks);
}
