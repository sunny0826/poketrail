#include "support/adventure_ui/harness.hpp"
#include "support/capture_badge.hpp"
#include "support/rendered_text.hpp"
#include "battle_presentation.hpp"
#include <algorithm>
#include <functional>
#include <zlib.h>

namespace {
void writeBigEndian32(std::vector<uint8_t>& out, uint32_t value) {
  out.push_back(uint8_t((value >> 24) & 0xff));
  out.push_back(uint8_t((value >> 16) & 0xff));
  out.push_back(uint8_t((value >> 8) & 0xff));
  out.push_back(uint8_t(value & 0xff));
}

void writeChunk(FILE* file, const char type[4], const uint8_t* data, size_t size) {
  std::vector<uint8_t> header;
  writeBigEndian32(header, uint32_t(size));
  std::fwrite(header.data(), 1, header.size(), file);
  std::fwrite(type, 1, 4, file);
  if (size) std::fwrite(data, 1, size, file);
  uLong crc = crc32(0L, reinterpret_cast<const Bytef*>(type), 4);
  if (size) crc = crc32(crc, data, uInt(size));
  std::vector<uint8_t> tail;
  writeBigEndian32(tail, uint32_t(crc));
  std::fwrite(tail.data(), 1, tail.size(), file);
}

void savePng(const char* name) {
  constexpr int kSize = 468;
  std::vector<uint8_t> raw(size_t(kSize) * (1 + kSize * 3));
  for (int y = 0; y < kSize; ++y) {
    uint8_t* row = raw.data() + size_t(y) * (1 + kSize * 3);
    row[0] = 0;
    for (int x = 0; x < kSize; ++x) {
      const uint16_t p = M5.Display.readPixel(x, y);
      row[1 + x * 3 + 0] = uint8_t((p >> 11) * 255 / 31);
      row[1 + x * 3 + 1] = uint8_t(((p >> 5) & 63) * 255 / 63);
      row[1 + x * 3 + 2] = uint8_t((p & 31) * 255 / 31);
    }
  }
  uLongf compressedSize = compressBound(uLong(raw.size()));
  std::vector<uint8_t> compressed(compressedSize);
  check(compress2(compressed.data(), &compressedSize, raw.data(), uLong(raw.size()), Z_BEST_SPEED) == Z_OK,
        "compress PNG screenshot");
  const std::string path = verificationOutput((std::string(name) + ".png").c_str());
  FILE* file = std::fopen(path.c_str(), "wb");
  check(file != nullptr, "open PNG evidence");
  static const uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  std::fwrite(signature, 1, sizeof(signature), file);
  std::vector<uint8_t> ihdr;
  writeBigEndian32(ihdr, kSize);
  writeBigEndian32(ihdr, kSize);
  ihdr.push_back(8);
  ihdr.push_back(2);
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);
  writeChunk(file, "IHDR", ihdr.data(), ihdr.size());
  writeChunk(file, "IDAT", compressed.data(), compressedSize);
  writeChunk(file, "IEND", nullptr, 0);
  std::fclose(file);
}

void snapBoth(Runtime& r, const char* name) {
  snapCurrent(r, name);
  savePng(name);
}

class ObservingStore : public MemoryStore {
 public:
  std::function<void(const Save&)> onCommit;
  CommitStatus commit(const Save& s) override {
    if (onCommit) onCommit(s);
    return MemoryStore::commit(s);
  }
};

void savedVictory(MemoryStore& store, Save& s, bool captured) {
  s.bootGeneration = 1; s.starter = s.partner = 1; setOwned(s, 1); setRegistered(s, 1);
  s.rosterRevision = 1; s.encounterTarget = 300;
  s.totalSteps = s.undatedSteps = 5000; s.explorationSpent = 5000; s.nextEncounterId = 2;
  s.phase = Phase::ThrowReady; s.result = Result::Won;
  s.encounter.id = 1; s.encounter.species = s.encounter.partner = 1; s.encounter.route = Route::Kanto;
  while ((throwRoll(s.encounter, 1, s.encounter.ruleVersion) >=
          captureProbability(s.encounter.ruleVersion, catalogEntry(1)->rare, Timing::Perfect)) == captured)
    ++s.encounter.seed;
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "saved victory fixture validates and encodes");
}

void savedEncounter(MemoryStore& store, Save& s) {
  s.bootGeneration = 1; s.starter = s.partner = 1; setOwned(s, 1); setRegistered(s, 1);
  s.rosterRevision = 1; s.encounterTarget = 300;
  s.totalSteps = s.undatedSteps = 5000; s.explorationSpent = 5000; s.nextEncounterId = 2;
  s.phase = Phase::EncounterReady; s.result = Result::None;
  s.encounter.id = 1; s.encounter.species = 25; s.encounter.partner = 1;
  s.encounter.route = Route::Kanto; s.encounter.seed = 7;
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "saved encounter fixture validates and encodes");
}

void enterBattle(Runtime& r, TestPlatform& p) {
  openMenu(r, p);
  select(r, p, MenuItem::Encounter);
  check(r.screen().page == Page::Encounter, "encounter menu opens encounter page");
  select(r, p, 0);
  check(r.screen().page == Page::Battle, "starting battle opens battle page");
}

void playUntilBeforeFinalWinHit(Runtime& r, TestPlatform& p) {
  const uint32_t limit = host_time_ms + 40000;
  while (r.engine().battleView().active && int32_t(limit - host_time_ms) > 0) {
    const auto v = r.engine().battleView();
    if (!v.consumed && !countdownDigit(v, host_time_ms) && int32_t(v.targetMs - host_time_ms) <= 20) {
      if (v.expected == Action::Attack && v.enemyHp <= 2) return;
      const uint32_t dt = host_time_ms < v.targetMs ? v.targetMs - host_time_ms : 0;
      if (v.expected == Action::Attack) p.next.right = true;
      else p.next.left = true;
      p.next.held = true;
      frame(r, dt);
      p.next = Input();
      frame(r, 20);
    } else {
      frame(r, 20);
    }
  }
  check(false, "expected to reach final winning blow cue");
}

void slowSaveCapture() {
  for (bool captured : {false, true}) {
    MemoryStore store; Save s; savedVictory(store, s, captured);
    TestPlatform p; Runtime r(store, p); start(r);
    store.delayMs = 5000;
    p.next.held = p.next.touch = true; p.next.x = 233; p.next.y = 344; frame(r);
    const uint32_t target = r.engine().throwView().targetMs;
    while (int32_t(target - host_time_ms) > 0) {
      p.next.held = p.next.touchDown = true;
      frame(r, std::min(20u, target - host_time_ms));
    }
    p.next.click = true; p.next.x = 233; p.next.y = 344; frame(r, 0);
    const auto& attempt = r.engine().view().encounter.attempts[0];
    check(attempt.used && attempt.timing == Timing::Perfect && attempt.captured == captured,
          "slow save preserves release-time timing and the committed outcome");
    check(r.screen().page == Page::Throw && r.screen().capture.active &&
          captureFrameAt(r.screen().capture, host_time_ms).stage == CaptureStage::Flight,
          "slow save cannot consume the flight before its first rendered frame");
    snapBoth(r, captured ? "slow-save-touch-flight-success" : "slow-save-touch-flight-escape");
    while (r.screen().capture.active) frame(r);
    check(r.screen().page == (captured ? Page::Result : Page::Throw),
          "slow-save animation finishes on the page for its saved outcome");
  }
}

void slowSaveBattleFinishingBlow() {
  // 1. Victory finishing blow + 800 ms slow save + pre-commit throw transition frame.
  {
    ObservingStore store; Save s; savedEncounter(store, s);
    TestPlatform p; Runtime r(store, p); start(r);
    enterBattle(r, p);
    playUntilBeforeFinalWinHit(r, p);

    const auto v = r.engine().battleView();
    const uint32_t dt = host_time_ms < v.targetMs ? v.targetMs - host_time_ms : 0;
    const unsigned feedbacksBefore = p.feedbacks;
    const unsigned commitsBefore = store.commits;
    store.delayMs = 800;
    p.next.right = true; p.next.held = true;
    frame(r, dt);
    p.next = Input();

    check(store.commits == commitsBefore + 1, "winning blow immediately commits result");
    check(!r.engine().savePending() && r.engine().view().phase == Phase::ThrowReady &&
          r.engine().view().result == Result::Won, "winning blow commits ThrowReady state");
    check(r.screen().page == Page::Battle && r.screen().battleEffectOverride,
          "800ms slow save keeps screen on Page::Battle to play finishing blow effect");
    check(!r.canSwitchMode(), "safe() stays false while finishing effect plays");
    check(p.feedbacks > feedbacksBefore, "finishing blow triggers vibration feedback after commit");
    snapBoth(r, "battle-finishing-win-start");

    idle(r, 140);
    check(r.screen().page == Page::Battle,
          "Page::Battle remains visible at 140ms into the 450ms finishing effect");
    snapBoth(r, "battle-finishing-win-mid");

    idle(r, 320);
    check(r.screen().page == Page::Throw && !r.screen().battleEffectOverride,
          "Page::Battle smoothly transitions to Page::Throw after 450ms finishing effect");
    snapBoth(r, "battle-finishing-win-to-throw");
    frame(r);

    // Verify immediate throw-release transition frame before slow commit.
    p.next.right = true; p.next.held = true; frame(r);
    check(r.screen().charging && r.engine().throwView().active, "holding right starts throw charge");
    const uint32_t target = r.engine().throwView().targetMs;
    while (int32_t(target - host_time_ms) > 0) {
      p.next.held = p.next.rightDown = true;
      frame(r, std::min(20u, target - host_time_ms));
    }
    bool sawCommitting = false;
    store.delayMs = 800;
    store.onCommit = [&](const Save& pending) {
      sawCommitting = true;
      check(r.screen().page == Page::Throw && r.screen().throwCommitting && !r.screen().capture.active,
            "throw release draws committing transition frame before store commit");
      check(capture_badge_test::iconAt(kBallX, kHandY - 10),
            "committing transition frame lifts the pokeball cleanly");
      check(!rendered_text::contains("捕获成功", theme::kGold) &&
            !rendered_text::contains("挣脱了", theme::kText),
            "committing transition frame never reveals capture outcome early");
      check(pending.encounter.usedBalls == 1 && r.engine().view().encounter.usedBalls == 0,
            "published save remains uncommitted while transition frame is shown");
      snapBoth(r, "throw-release-committing");
    };
    p.next = Input();
    frame(r, 0);
    store.onCommit = nullptr;
    check(sawCommitting, "throw release exercised pre-commit transition draw");
    check(!r.screen().throwCommitting, "throwCommitting clears immediately after pressThrow");
    check(r.screen().page == Page::Throw && r.screen().capture.active &&
          captureFrameAt(r.screen().capture, host_time_ms).stage == CaptureStage::Flight,
          "slow save after throw starts CaptureStage::Flight from elapsed=0");
    check(capture_badge_test::iconAt(kBallX, kHandY),
          "first CaptureStage::Flight frame draws ball at kHandY");
    snapBoth(r, "throw-flight-after-slow-commit");
  }

  // 2. Defeat finishing blow via timeout miss + 800 ms slow save.
  {
    ObservingStore store; Save s; savedEncounter(store, s);
    TestPlatform p; Runtime r(store, p); start(r);
    enterBattle(r, p);
    while (r.engine().battleView().active && r.engine().battleView().playerHp > 1) {
      frame(r, 20);
    }
    while (r.engine().battleView().active && r.engine().battleView().expected != Action::Dodge) {
      frame(r, 20);
    }
    check(r.engine().battleView().active && r.engine().battleView().playerHp == 1 &&
          r.engine().battleView().expected == Action::Dodge,
          "defeat test reaches final dodge cue at 1 HP");
    const unsigned feedbacksBefore = p.feedbacks;
    const unsigned commitsBefore = store.commits;
    store.delayMs = 800;
    while (store.commits == commitsBefore) frame(r, 20);

    check(!r.engine().savePending() && r.engine().view().phase == Phase::Resolved &&
          r.engine().view().result == Result::Lost, "defeat finishing miss commits Lost state");
    check(r.screen().page == Page::Battle && r.screen().battleEffectOverride,
          "800ms slow save keeps defeat on Page::Battle to play hurt finishing effect");
    check(!r.canSwitchMode(), "safe() stays false during defeat finishing effect");
    check(p.feedbacks > feedbacksBefore, "defeat finishing miss triggers vibration feedback");
    snapBoth(r, "battle-finishing-loss-start");

    idle(r, 140);
    check(r.screen().page == Page::Battle,
          "defeat stays on Page::Battle at 140ms into finishing effect");

    idle(r, 320);
    check(r.screen().page == Page::Result && r.engine().view().result == Result::Lost &&
          !r.screen().battleEffectOverride,
          "defeat transitions to Page::Result after 450ms finishing effect");
    snapBoth(r, "battle-finishing-loss-to-result");
  }

  // 3. Save failure on finishing blow and throw release -> immediate Page::Error -> retry succeeds.
  {
    ObservingStore store; Save s; savedEncounter(store, s);
    TestPlatform p; Runtime r(store, p); start(r);
    enterBattle(r, p);
    playUntilBeforeFinalWinHit(r, p);

    const auto v = r.engine().battleView();
    const uint32_t dt = host_time_ms < v.targetMs ? v.targetMs - host_time_ms : 0;
    store.delayMs = 800;
    store.fail = true;
    p.next.right = true; p.next.held = true;
    frame(r, dt);
    p.next = Input();
    frame(r);

    check(r.engine().savePending() && r.engine().view().phase == Phase::Battle,
          "failed finishing blow keeps savePending and pre-commit Battle phase");
    check(r.screen().page == Page::Error && !r.screen().battleEffectOverride,
          "failed finishing blow immediately enters Page::Error without activating battle override");
    snapBoth(r, "battle-finishing-save-error");

    store.fail = false;
    store.delayMs = 0;
    frame(r);
    select(r, p, 0);
    check(!r.engine().savePending() && r.engine().view().phase == Phase::ThrowReady &&
          r.engine().view().result == Result::Won && r.screen().page == Page::Throw,
          "retrying failed finishing blow enters Page::Throw cleanly");

    // Failed throw release transitions to Page::Error without leaking reward, and retry recovers cleanly.
    p.next.right = true; p.next.held = true; frame(r);
    const uint32_t target = r.engine().throwView().targetMs;
    while (int32_t(target - host_time_ms) > 0) {
      p.next.held = p.next.rightDown = true;
      frame(r, std::min(20u, target - host_time_ms));
    }
    store.delayMs = 800;
    store.fail = true;
    p.next = Input();
    frame(r, 0);

    check(!r.screen().throwCommitting && !r.screen().capture.active,
          "failed throw clears throwCommitting and starts no capture animation");
    check(r.screen().page == Page::Error && r.engine().savePending(),
          "failed throw enters Page::Error with savePending");
    check(r.engine().view().encounter.usedBalls == 0 && !owns(r.engine().view(), 25),
          "failed throw preserves pre-throw state and grants no reward");

    store.fail = false;
    store.delayMs = 0;
    frame(r);
    select(r, p, 0);
    check(!r.engine().savePending() && r.engine().view().encounter.usedBalls == 1,
          "retrying failed throw commits the single frozen attempt");
    const bool captured = r.engine().view().result == Result::Captured;
    check(r.screen().page == (captured ? Page::Result : Page::Throw),
          "retrying failed throw enters the page matching its committed outcome");
  }
}
}
int main() {
  c152::begin("CAPTURE TEST");
  slowSaveCapture();
  slowSaveBattleFinishingBlow();
  std::printf("%u capture Runtime checks, 0 failures\n", checks);
}
