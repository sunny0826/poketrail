#include "support/motion_fixture.hpp"
#include <limits>

namespace {
using pokedex::motion::Detector;
using pokedex::motion::StepBatch;
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
  for (float amplitude : {0.20f, 0.12f, 0.10f, 0.08f}) {
    Feed light;
    light.rest(200);
    for (int i = 0; i < 10; ++i) light.cycleAmp(500, amplitude);
    check(light.steps.size() >= 9, "light gait still counted");
  }
  for (float companion : {0.30f, 0.10f}) {
    Feed mixed;
    mixed.rest(200);
    for (int i = 0; i < 40; ++i) mixed.cycleAmp(500, i % 2 ? companion : 0.08f);
    check(mixed.steps.size() >= 38, "alternating weak/stronger peaks keep counting");
    check(mixed.gaps == 0, "alternating tier gait keeps sampling continuous");
  }
  for (float impact : {0.15f, 0.20f, 0.30f}) {
    Feed pocket;
    for (int s = 0; s < 40; ++s)
      for (unsigned t = 0; t < 500; t += 20)
        pocket.sample(1.0f + impact * cosf(6.2831853f * float(t) / 110.0f) * expf(-float(t) / 70.0f));
    check(pocket.steps.size() >= 38, "pocket impact steps counted at walking cadence");
  }
  Feed handheld;
  uint32_t jitter = 777;
  handheld.rest(200);
  for (int i = 0; i < 10; ++i)
    for (unsigned t = 0; t < 500; t += 20) {
      jitter = jitter * 1664525u + 1013904223u;
      handheld.sample(1.0f - 0.30f * cosf(6.28318530718f * float(t) / 500.0f)
                      + float(int(jitter % 101) - 50) / 1000);
    }
  check(handheld.steps.size() >= 9, "handheld jitter keeps walking cadence");
  Feed pair;
  pair.cycle(); pair.cycle();
  check(pair.steps.size() == 2, "two strong peaks confirmed as two steps");
  Feed single;
  single.cycle();
  check(single.steps.empty(), "single peak stays unconfirmed");
  Feed varying;
  for (int i = 0; i < 3; ++i) { varying.cycle(500); varying.cycle(650); varying.cycle(430); }
  check(varying.steps.size() >= 8, "varying cadence keeps counting");
  Feed paused;
  paused.cycle(); paused.cycle(); paused.cycle();
  check(paused.steps.size() == 3, "three strong peaks counted");
  paused.rest(2000);
  size_t before = paused.steps.size();
  paused.cycle(); paused.cycle();
  check(paused.steps.size() == before + 2, "restart after 2s pause counted");
  paused.rest(4000);
  before = paused.steps.size();
  paused.cycle(); paused.cycle();
  check(paused.steps.size() == before + 2, "restart after 4s pause counted");
  Feed noise;
  uint32_t random = 12345;
  for (int i = 0; i < 2000; ++i) {
    random = random * 1664525u + 1013904223u;
    noise.sample(1.0f + float(int(random % 181) - 90) / 1000);
  }
  check(noise.steps.empty(), "bounded irregular noise no steps");
  Feed taps;
  for (int i = 0; i < 6; ++i) { taps.cycle(); taps.rest(3000); }
  check(taps.steps.empty(), "isolated taps with long rest no steps");
  Feed suppressed;
  suppressed.cycle(); suppressed.cycle();
  before = suppressed.steps.size();
  suppressed.detector.suppressUntil(suppressed.now + 2000);
  suppressed.detector.suppressUntil(suppressed.now + 1000);
  for (int i = 0; i < 4; ++i) suppressed.cycle();
  check(suppressed.steps.size() == before, "vibration suppressed, shorter call cannot shorten window");
  for (int i = 0; i < 3; ++i) suppressed.cycle();
  check(suppressed.steps.size() == before + 3, "fresh confirmation after suppression");
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
    const size_t base = f.steps.size();
    check(f.sample(invalid).qualityGap, "invalid sample reports quality gap");
    f.cycle();
    check(f.steps.size() == base, "invalid sample discards old candidates");
    f.cycle();
    check(f.steps.size() == base + 2, "recovery after invalid sample");
  }
  for (uint32_t gap : {80u, 5000u, UINT32_MAX - 40u}) {
    Feed f;
    f.cycle(); f.cycle();
    const size_t base = f.steps.size();
    f.now += gap;
    check(f.sample(1).qualityGap, "time gap/backward reports quality gap");
    f.cycle();
    check(f.steps.size() == base, "gap clears candidate cadence");
    f.cycle();
    check(f.steps.size() == base + 2, "cadence resumes after gap");
  }
  Feed duplicate;
  duplicate.sample(1);
  duplicate.now -= 20;
  check(duplicate.sample(1).qualityGap, "duplicate timestamp invalidates cadence");
}
void stallTests() {
  for (float crest : {1.10f, 1.30f}) {
    Feed held;
    held.sampleFiltered(1.0f);
    for (int i = 0; i < 2; ++i)
      for (unsigned t = 0; t < 1600; t += 20)
        held.sampleFiltered(t == 0 || t == 140 ? 0.90f : (t <= 120 ? crest : 1.0f));
    held.sampleFiltered(0.90f);
    held.sampleFiltered(crest);
    for (unsigned t = 0; t < 10000; t += 20) held.sampleFiltered(1.0f);
    check(held.steps.size() == 3, "crest held above the valley is still confirmed");
    check(held.maxAge <= pokedex::motion::kMaxConfirmationDelayMs,
          "held crest stays inside the clock attribution window");
    const size_t before = held.steps.size();
    held.sampleFiltered(0.90f);
    check(held.steps.size() == before, "the valley after the stall releases no duplicate");
    check(held.gaps == 0, "held crest keeps sampling continuous");
  }
  for (float level : {1.06f, 1.10f, 1.20f, 1.5f}) {
    Feed pressed;
    pressed.sampleFiltered(1.0f);
    pressed.sampleFiltered(level);
    for (unsigned t = 0; t < 30000; t += 20) pressed.sampleFiltered(level);
    check(pressed.steps.empty(), "sustained high load alone never confirms a step");
    check(pressed.gaps == 0, "sustained high load keeps sampling continuous");
    pressed.sampleFiltered(0.90f);
    for (int i = 0; i < 2; ++i)
      for (unsigned t = 0; t < 500; t += 20)
        pressed.sampleFiltered(1.0f - 0.30f * std::cos(6.28318530718f * float(t) / 500));
    check(pressed.steps.size() == 2, "counting resumes once the pressure drops to the valley");
  }
}
void ownershipTests() {
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
}
int main() {
  motionTests();
  stallTests();
  ownershipTests();
  std::printf("PASS motion: %u checks (synthetic signals only)\n", checks);
}
