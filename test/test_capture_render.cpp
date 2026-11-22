#include "support/adventure_ui/harness.hpp"
#include "support/capture_badge.hpp"
#include "support/rendered_text.hpp"
#include "battle_presentation.hpp"
#include <algorithm>
#include <functional>
#include <zlib.h>

namespace {
void savePng(const char* name) {
  constexpr int N = 468; std::vector<uint8_t> raw(N * (1 + N * 3), 0);
  for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
    uint16_t p = M5.Display.readPixel(x, y); uint8_t* r = &raw[y * (1 + N * 3) + 1 + x * 3];
    r[0] = (p >> 11) * 255 / 31; r[1] = ((p >> 5) & 63) * 255 / 63; r[2] = (p & 31) * 255 / 31;
  }
  uLongf zlen = compressBound(uLong(raw.size())); std::vector<uint8_t> z(zlen);
  check(compress2(z.data(), &zlen, raw.data(), uLong(raw.size()), Z_BEST_SPEED) == Z_OK, "png");
  FILE* f = std::fopen(verificationOutput((std::string(name) + ".png").c_str()).c_str(), "wb");
  check(f != nullptr, "open");
  static const uint8_t sig[8] = {137,80,78,71,13,10,26,10}, hdr[13] = {0,0,1,212,0,0,1,212,8,2,0,0,0};
  std::fwrite(sig, 1, 8, f);
  auto w = [&](const char t[4], const uint8_t* d, size_t n) {
    uint8_t b[4] = {uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)};
    std::fwrite(b,1,4,f); std::fwrite(t,1,4,f); if (n) std::fwrite(d,1,n,f);
    uLong c = crc32(0, reinterpret_cast<const Bytef*>(t), 4); c = n ? crc32(c, d, uInt(n)) : c;
    uint8_t q[4] = {uint8_t(c>>24),uint8_t(c>>16),uint8_t(c>>8),uint8_t(c)}; std::fwrite(q,1,4,f);
  };
  w("IHDR", hdr, 13); w("IDAT", z.data(), zlen); w("IEND", nullptr, 0); std::fclose(f);
}

void snapBoth(Runtime& r, const char* name) { snapCurrent(r, name); savePng(name); }

void verifyPartial(Screens& c, Screens& ref, const ScreenState& s, const Engine& e,
                   uint32_t now, const char* lbl, const char* png = nullptr) {
  check(c.draw(s, e, {}, now), lbl);
  const auto got = M5.Display.framebuffer();
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x)
    if ((x - 233) * (x - 233) + (y - 233) * (y - 233) > 228 * 228)
      check(M5.Display.readPixel(x, y) == TFT_BLACK, lbl);
  if (png) savePng(png);
  ScreenState f = s;
  f.page = s.page == Page::Settings ? Page::Pause : Page::Settings;
  check(ref.draw(f, e, {}, now) && ref.draw(s, e, {}, now), lbl);
  check(got == M5.Display.framebuffer(), lbl);
}

void holdThrow(Runtime& r, TestPlatform& p, int32_t stopRem) {
  p.next.right = p.next.held = true; frame(r);
  const uint32_t target = r.engine().throwView().targetMs;
  while (int32_t(target - host_time_ms) > stopRem) {
    p.next.held = p.next.rightDown = true;
    frame(r, std::min(20u, uint32_t(int32_t(target - host_time_ms) - stopRem)));
  }
}

void pressSide(Runtime& r, TestPlatform& p, bool right, uint32_t dt = 0) {
  p.next.right = right; p.next.left = !right; p.next.held = true;
  frame(r, dt); p.next = Input(); frame(r, 20);
}

bool hasHoldPrompt(uint16_t color) {
  return rendered_text::contains("按住右键蓄力 · 松手投球", color, 370, 440);
}

class ObservingStore : public MemoryStore {
 public:
  std::function<void(const Save&)> onCommit;
  CommitStatus commit(const Save& s) override {
    if (onCommit) onCommit(s);
    return MemoryStore::commit(s);
  }
};

void initSaveBase(Save& s, Phase phase, Result res, uint16_t species) {
  s.bootGeneration = 1; s.starter = s.partner = 1; setOwned(s, 1); setRegistered(s, 1);
  s.rosterRevision = 1; s.encounterTarget = 300;
  s.totalSteps = s.undatedSteps = 5000; s.explorationSpent = 5000; s.nextEncounterId = 2;
  s.phase = phase; s.result = res;
  s.encounter.id = 1; s.encounter.species = species; s.encounter.partner = 1;
  s.encounter.route = Route::Kanto; s.encounter.seed = 7;
}

void savedVictory(MemoryStore& store, Save& s, bool captured) {
  initSaveBase(s, Phase::ThrowReady, Result::Won, 1); s.encounter.seed = 0;
  while ((throwRoll(s.encounter, 1, s.encounter.ruleVersion) >=
          captureProbability(s.encounter.ruleVersion, catalogEntry(1)->rare, Timing::Perfect)) == captured)
    ++s.encounter.seed;
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "saved victory fixture");
}

void savedEncounter(MemoryStore& store, Save& s) {
  initSaveBase(s, Phase::EncounterReady, Result::None, 25);
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "saved encounter fixture");
}

void enterBattle(Runtime& r, TestPlatform& p) {
  openMenu(r, p); select(r, p, MenuItem::Encounter);
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
      pressSide(r, p, v.expected == Action::Attack, host_time_ms < v.targetMs ? v.targetMs - host_time_ms : 0);
    } else frame(r, 20);
  }
  check(false, "expected final winning blow cue");
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
  auto winHit = [&](ObservingStore& store, Save& s, TestPlatform& p, Runtime& r, bool fail) {
    savedEncounter(store, s); start(r); enterBattle(r, p); playUntilBeforeFinalWinHit(r, p);
    const auto v = r.engine().battleView();
    store.delayMs = 800; store.fail = fail;
    p.next.right = p.next.held = true;
    frame(r, host_time_ms < v.targetMs ? v.targetMs - host_time_ms : 0);
    p.next = Input();
  };
  {
    ObservingStore store; Save s; TestPlatform p; Runtime r(store, p);
    const unsigned fb0 = p.feedbacks, cm0 = store.commits;
    winHit(store, s, p, r, false);
    check(store.commits > cm0 && !r.engine().savePending() && r.engine().view().phase == Phase::ThrowReady &&
          r.engine().view().result == Result::Won && r.screen().page == Page::Battle &&
          r.screen().battleEffectOverride && !r.canSwitchMode() && p.feedbacks > fb0,
          "winning blow commits ThrowReady while playing finishing effect");
    snapBoth(r, "battle-finishing-win-start");
    idle(r, 140); check(r.screen().page == Page::Battle, "Battle visible at 140ms"); snapBoth(r, "battle-finishing-win-mid");
    idle(r, 320); check(r.screen().page == Page::Throw && !r.screen().battleEffectOverride, "Throw after 450ms");
    snapBoth(r, "battle-finishing-win-to-throw"); frame(r);

    holdThrow(r, p, 0);
    check(r.screen().charging && r.engine().throwView().active, "throw charge starts");
    bool sawCommit = false; store.delayMs = 800;
    store.onCommit = [&](const Save& pending) {
      sawCommit = true;
      check(r.screen().page == Page::Throw && r.screen().throwCommitting && !r.screen().capture.active &&
            capture_badge_test::iconAt(kBallX, kHandY - 10) &&
            !rendered_text::contains("捕获成功", theme::kGold) && !rendered_text::contains("挣脱了", theme::kText) &&
            pending.encounter.usedBalls == 1 && r.engine().view().encounter.usedBalls == 0,
            "committing frame lifts ball without revealing outcome");
      snapBoth(r, "throw-release-committing");
    };
    p.next = Input(); frame(r, 0); store.onCommit = nullptr;
    check(sawCommit && !r.screen().throwCommitting && r.screen().page == Page::Throw && r.screen().capture.active &&
          captureFrameAt(r.screen().capture, host_time_ms).stage == CaptureStage::Flight &&
          capture_badge_test::iconAt(kBallX, kHandY),
          "slow save starts Flight from elapsed=0 at kHandY");
    snapBoth(r, "throw-flight-after-slow-commit");
  }
  {
    ObservingStore store; Save s; savedEncounter(store, s);
    TestPlatform p; Runtime r(store, p); start(r); enterBattle(r, p);
    while (r.engine().battleView().active &&
           (r.engine().battleView().playerHp > 1 || r.engine().battleView().expected != Action::Dodge))
      frame(r, 20);
    check(r.engine().battleView().active && r.engine().battleView().playerHp == 1 &&
          r.engine().battleView().expected == Action::Dodge, "defeat reaches 1 HP dodge");
    const unsigned fb0 = p.feedbacks, cm0 = store.commits;
    store.delayMs = 800;
    while (store.commits == cm0) frame(r, 20);
    check(!r.engine().savePending() && r.engine().view().phase == Phase::Resolved &&
          r.engine().view().result == Result::Lost && r.screen().page == Page::Battle &&
          r.screen().battleEffectOverride && !r.canSwitchMode() && p.feedbacks > fb0,
          "defeat miss commits Lost while playing hurt effect");
    snapBoth(r, "battle-finishing-loss-start");
    idle(r, 140); check(r.screen().page == Page::Battle, "defeat visible at 140ms");
    idle(r, 320);
    check(r.screen().page == Page::Result && r.engine().view().result == Result::Lost && !r.screen().battleEffectOverride,
          "defeat enters Result after 450ms");
    snapBoth(r, "battle-finishing-loss-to-result");
  }
  {
    ObservingStore store; Save s; TestPlatform p; Runtime r(store, p);
    winHit(store, s, p, r, true); frame(r);
    check(r.engine().savePending() && r.engine().view().phase == Phase::Battle &&
          r.screen().page == Page::Error && !r.screen().battleEffectOverride, "failed blow enters Error");
    snapBoth(r, "battle-finishing-save-error");

    store.fail = false; store.delayMs = 0; frame(r); select(r, p, 0);
    check(!r.engine().savePending() && r.engine().view().phase == Phase::ThrowReady &&
          r.engine().view().result == Result::Won && r.screen().page == Page::Throw,
          "retry finishing blow enters Throw");

    holdThrow(r, p, 0);
    store.delayMs = 800; store.fail = true; p.next = Input(); frame(r, 0);
    check(!r.screen().throwCommitting && !r.screen().capture.active &&
          r.screen().page == Page::Error && r.engine().savePending() &&
          r.engine().view().encounter.usedBalls == 0 && !owns(r.engine().view(), 25),
          "failed throw enters Error without leaking state or reward");

    store.fail = false; store.delayMs = 0; frame(r); select(r, p, 0);
    check(!r.engine().savePending() && r.engine().view().encounter.usedBalls == 1 &&
          r.screen().page == (r.engine().view().result == Result::Captured ? Page::Result : Page::Throw),
          "retry throw commits frozen attempt to matching page");
  }
}

void throwOverchargeAndShortCancelFeedback() {
  {
    MemoryStore store; Save s; savedVictory(store, s, false);
    TestPlatform p; Runtime r(store, p); start(r);
    p.next.right = p.next.held = true; frame(r);
    for (int i = 0; i < 4; ++i) { p.next.rightDown = p.next.held = true; frame(r, 20); }
    p.next = Input(); frame(r, 20);
    check(!r.screen().charging && !r.engine().throwView().active && !r.engine().view().encounter.usedBalls &&
          throwCancelNotice(r.screen(), host_time_ms) && hasHoldPrompt(theme::kGold) && !hasHoldPrompt(theme::kMint),
          "short hold cancels and pulses prompt gold");
    savePng("throw-short-cancel-gold");
    idle(r, 520);
    check(throwCancelNotice(r.screen(), host_time_ms) && hasHoldPrompt(theme::kGold), "gold before 600ms");
    idle(r, 100);
    check(!throwCancelNotice(r.screen(), host_time_ms) && hasHoldPrompt(theme::kMint) && !hasHoldPrompt(theme::kGold),
          "mint after 600ms");
    savePng("throw-short-cancel-restored");

    holdThrow(r, p, -90);
    const int32_t rem = int32_t(r.engine().throwView().targetMs - host_time_ms);
    const int warnR = std::max(16, 20 + int(rem) * 4 / 180);
    check(rem < 0 && M5.Display.readPixel(233 + 24, 233) == theme::kRed &&
          M5.Display.readPixel(233 + warnR, 233) == theme::kRed, "overcharge red ring");
    savePng("throw-overcharge-red");
  }
  MemoryStore store; Save s; savedVictory(store, s, false);
  Engine e(store); check(e.begin() == CommandResult::Applied, "eq throw");
  Screens c, ref; check(c.begin() && ref.begin(), "eq scr");
  ScreenState st; st.page = Page::Throw;
  auto eq = [&](uint32_t t, const char* l) { verifyPartial(c, ref, st, e, t, l); };
  eq(1000, "throw idle");
  const auto key = AttemptKey(e.view().encounter.id, 1);
  check(e.beginThrow(key, 1020) == CommandResult::Applied, "eq begin");
  eq(1500, "throw charge"); eq(2610, "throw overcharge");
  check(e.cancelThrow(key) == CommandResult::Applied, "eq cancel");
  st.throwCancelNoticeActive = true; st.throwCancelNoticeUntilMs = 3210;
  eq(2650, "throw gold"); eq(3250, "throw expired");
  st.page = Page::PracticeThrow; st.practiceSpecies = 25; st.throwCancelNoticeUntilMs = 4000;
  eq(3500, "practice gold"); eq(4050, "practice expired");
}

void battleMissReasonAndEasing() {
  for (bool en : {false, true}) {
    MemoryStore store; Save s; savedEncounter(store, s);
    s.language = en ? i18n::Language::English : i18n::Language::Chinese;
    check(store.commit(s) == CommitStatus::Committed, "save lang");
    TestPlatform p; Runtime r(store, p); start(r); enterBattle(r, p);
    auto checkMiss = [&](bool match, MissReason want, const char* zh, const char* enText, const char* png) {
      while (r.engine().battleView().consumed || countdownDigit(r.engine().battleView(), host_time_ms) ||
             (!match && int32_t(r.engine().battleView().targetMs - host_time_ms) > 20))
        frame(r, 20);
      pressSide(r, p, (r.engine().battleView().expected == Action::Attack) == match);
      check(r.engine().battleView().missReason == want &&
            rendered_text::contains(en ? enText : zh, theme::kMuted, 370, 440), png);
      savePng(png);
    };
    checkMiss(true, MissReason::Early, "太早", "Early", en ? "battle-miss-early-en" : "battle-miss-early-zh");
    checkMiss(false, MissReason::WrongKey, "按错", "Wrong", en ? "battle-miss-wrong-en" : "battle-miss-wrong-zh");
  }
  MemoryStore store; Save s; savedEncounter(store, s);
  Engine e(store); check(e.begin() == CommandResult::Applied && e.startBattle(1000) == CommandResult::Applied, "ease");
  Screens c, ref; check(c.begin() && ref.begin(), "ease scr");
  ScreenState st; st.page = Page::Battle;
  auto eq = [&](uint32_t at, const char* png = nullptr) { verifyPartial(c, ref, st, e, at, "ease eq", png); };
  uint32_t t = 1000; bool atk = false, dge = false;
  for (int i = 0; i < 400 && (!atk || !dge); ++i) {
    t += 20; e.tick(t, {});
    const auto& bv = e.battleView();
    if (!bv.consumed && !countdownDigit(bv, t) && int32_t(bv.targetMs - t) <= 10) {
      e.press(bv.expected, t);
      if (e.battleView().settledCue == Action::Attack && !atk) {
        atk = true; eq(t + 40, "battle-easing-lunge");
        check(M5.Display.readPixel(kPartnerX + 22 + 37, kPartnerY + 67) == theme::rgb(0x6C, 0x68, 0x48), "ease lunge");
        for (uint32_t dt : {100u, 180u, 320u}) eq(t + dt);
      } else if (e.battleView().settledCue == Action::Dodge && !dge) {
        dge = true; eq(t + 60, "battle-easing-dodge");
        for (uint32_t dt : {120u, 240u}) eq(t + dt);
      }
    }
  }
  check(atk && dge, "easing checked");
}
}
int main() {
  c152::begin("CAPTURE TEST");
  slowSaveCapture();
  slowSaveBattleFinishingBlow();
  throwOverchargeAndShortCancelFeedback();
  battleMissReasonAndEasing();
  std::printf("%u capture Runtime checks, 0 failures\n", checks);
}
