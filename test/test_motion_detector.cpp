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
  // 召回优先：弱冲击只要持续约 100 ms 并连续出现即确认，轻步与松口袋不再漏记。
  for (float amplitude : {0.20f, 0.12f, 0.10f, 0.08f}) {
    Feed light;
    light.rest(200);
    for (int i = 0; i < 10; ++i) light.cycleAmp(500, amplitude);
    check(light.steps.size() >= 9, "light gait still counted");
  }
  // 混合档位：证据按档位向下兼容，弱峰不能截断更强峰的待确认候选。
  for (float companion : {0.30f, 0.10f}) {
    Feed mixed;
    mixed.rest(200);
    for (int i = 0; i < 40; ++i) mixed.cycleAmp(500, i % 2 ? companion : 0.08f);
    check(mixed.steps.size() >= 38, "alternating weak/stronger peaks keep counting");
    check(mixed.gaps == 0, "alternating tier gait keeps sampling continuous");
  }
  Feed mixedThree;
  mixedThree.rest(200);
  mixedThree.cycleAmp(500, 0.08f);
  mixedThree.cycleAmp(500, 0.10f);
  mixedThree.cycleAmp(500, 0.08f);
  check(mixedThree.steps.size() == 3, "mixed tiers confirm on the third peak");
  // 间隔放大到 140% 的候选峰仍被接受，但不构成节奏，需要下一次复检才确认。
  Feed irregularStart;
  irregularStart.rest(200);
  irregularStart.cycleAmp(500, 0.10f);
  irregularStart.cycleAmp(500, 0.10f);
  irregularStart.cycleAmp(900, 0.10f);
  check(irregularStart.steps.empty(), "off-rhythm candidate peaks do not confirm a cadence");
  irregularStart.cycleAmp(900, 0.10f);
  check(irregularStart.steps.size() == 3, "regular rhythm releases the waiting peaks");
  // 口袋/背包佩戴：信号是脚步冲击谐波（~9Hz 衰减振荡），低通滤波必须放行
  // 足够的冲击能量让谷值压过阈值，否则步数大量丢失。
  for (float impact : {0.15f, 0.20f, 0.30f}) {
    Feed pocket;
    for (int s = 0; s < 40; ++s)
      for (unsigned t = 0; t < 500; t += 20)
        pocket.sample(1.0f + impact * cosf(6.2831853f * float(t) / 110.0f) * expf(-float(t) / 70.0f));
    check(pocket.steps.size() >= 38, "pocket impact steps counted at walking cadence");
  }
  // 手持抖动叠加正常步态仍需计数。
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
  // 短距离：两步正常步行不再等待第三个峰；单个峰仍作为拿起/放下丢弃。
  Feed pair;
  pair.cycle(); pair.cycle();
  check(pair.steps.size() == 2, "two strong peaks confirmed as two steps");
  Feed single;
  single.cycle();
  check(single.steps.empty(), "single peak stays unconfirmed");
  // 变节奏与上下楼不再反复清空候选。
  Feed varying;
  for (int i = 0; i < 3; ++i) { varying.cycle(500); varying.cycle(650); varying.cycle(430); }
  check(varying.steps.size() >= 8, "varying cadence keeps counting");
  Feed stairs;
  for (int i = 0; i < 10; ++i) stairs.cycle(600);
  check(stairs.steps.size() >= 9, "stair climbing cadence counted");
  // 停顿后重新起步：2 s / 4 s 内的新峰都能确认。
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
  // 有界噪声与孤立脉冲不能形成 cadence。
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
  Feed explicitReset;
  explicitReset.cycle(); explicitReset.cycle();
  const size_t base = explicitReset.steps.size();
  explicitReset.detector.reset();
  explicitReset.cycle();
  check(explicitReset.steps.size() == base, "explicit reset clears candidates");
  explicitReset.cycle();
  check(explicitReset.steps.size() == base + 2, "reset rebuilds cadence");
}
void stallTests() {
  // reviewer 场景：两个中峰正常落谷后，第三峰过顶回到 1.0g 并保持 10 s 不落谷。
  // 跟踪必须有实际时限，否则最终落谷时会按 10 秒前的峰顶确认，释放出超过
  // kMaxConfirmationDelayMs 的事件，日期窗口推导随之失效。
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
    check(held.maxAge <= pokedex::motion::kMaxStepMs + 2 * 1600 + 20,
          "held crest is released at the tracking limit, not after the stall");
    check(held.maxAge <= pokedex::motion::kMaxConfirmationDelayMs,
          "held crest stays inside the clock attribution window");
    const size_t before = held.steps.size();
    held.sampleFiltered(0.90f);
    check(held.steps.size() == before, "the valley after the stall releases no duplicate");
    check(held.gaps == 0, "held crest keeps sampling continuous");
  }
  // 持续高压：只在跟踪时限达到时接受一次，之后必须停止跟踪并等待真正落谷，
  // 不能每 2.2 s 重复产步。
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
}
int main() {
  motionTests();
  stallTests();
  ownershipTests();
  std::printf("PASS motion: %u checks (synthetic signals only)\n", checks);
}
