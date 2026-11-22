#include "support/adventure_ui/clock_cases.hpp"
#include "support/rendered_text.hpp"
#include <zlib.h>

namespace {
void savePng(const char* name) {
  constexpr int N = 468;
  std::vector<uint8_t> raw(N * (1 + N * 3), 0);
  for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
    const uint16_t p = M5.Display.readPixel(x, y);
    uint8_t* r = &raw[y * (1 + N * 3) + 1 + x * 3];
    r[0] = (p >> 11) * 255 / 31; r[1] = ((p >> 5) & 63) * 255 / 63; r[2] = (p & 31) * 255 / 31;
  }
  uLongf zlen = compressBound(uLong(raw.size())); std::vector<uint8_t> z(zlen);
  check(compress2(z.data(), &zlen, raw.data(), uLong(raw.size()), Z_BEST_SPEED) == Z_OK, "png");
  FILE* f = std::fopen(verificationOutput((std::string(name) + ".png").c_str()).c_str(), "wb");
  check(f != nullptr, "open png");
  static const uint8_t sig[8] = {137,80,78,71,13,10,26,10}, hdr[13] = {0,0,1,212,0,0,1,212,8,2,0,0,0};
  std::fwrite(sig, 1, 8, f);
  auto w = [&](const char t[4], const uint8_t* d, size_t n) {
    uint8_t b[4] = {uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)};
    std::fwrite(b,1,4,f); std::fwrite(t,1,4,f); if (n) std::fwrite(d,1,n,f);
    uLong c = crc32(0, reinterpret_cast<const Bytef*>(t), 4); if (n) c = crc32(c, d, uInt(n));
    uint8_t q[4] = {uint8_t(c>>24),uint8_t(c>>16),uint8_t(c>>8),uint8_t(c)}; std::fwrite(q,1,4,f);
  };
  w("IHDR", hdr, 13); w("IDAT", z.data(), zlen); w("IEND", nullptr, 0); std::fclose(f);
}
void checkCircleBoundary(const char* label) {
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x) {
    const int dx = x - c152::kScreenCenter, dy = y - c152::kScreenCenter;
    if (dx * dx + dy * dy > c152::kVisibleRadius * c152::kVisibleRadius && M5.Display.readPixel(x, y) != TFT_BLACK) {
      std::fprintf(stderr, "overflow %s (%d,%d)\n", label, x, y);
      check(false, label);
    }
  }
}
bool rectsOverlap(const menu_layout::Rect& a, const menu_layout::Rect& b) {
  return a.width > 0 && a.height > 0 && b.width > 0 && b.height > 0 &&
         a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}
void verifyPartialEq(Screens& c, const ScreenState& s, const Engine& e,
                     uint32_t now, const char* lbl, const char* png = nullptr) {
  check(c.draw(s, e, {}, now), lbl);
  const auto got = M5.Display.framebuffer();
  checkCircleBoundary(lbl);
  if (png) savePng(png);
  Screens fresh;
  check(fresh.begin() && fresh.draw(s, e, {}, now), lbl);
  if (got != M5.Display.framebuffer()) {
    for (size_t i = 0; i < got.size(); ++i) if (got[i] != M5.Display.framebuffer()[i]) {
      std::fprintf(stderr, "mismatch %s (%zu,%zu)\n", lbl, i % 468, i / 468); break;
    }
    check(false, lbl);
  }
}
void seedHomeFixture(MemoryStore& store, Phase phase, uint8_t queueCount, bool evolvable, bool captureBadges) {
  Save s; s.bootGeneration = 1; s.starter = s.partner = 1;
  setOwned(s, 1); setRegistered(s, 1); s.rosterRevision = 1; s.encounterTarget = 300;
  const uint64_t encCount = phase == Phase::Walking ? 0u : uint64_t(1 + queueCount);
  s.explorationSpent = encCount * 300u;
  s.totalSteps = captureBadges ? 105000u : std::max<uint64_t>(evolvable ? 7500u : 500u, s.explorationSpent);
  s.explorationCredit = s.totalSteps - s.explorationSpent;
  s.undatedSteps = captureBadges ? s.totalSteps - 5200u : s.totalSteps;
  s.progress[0].walked = evolvable ? 7500u : 500u;
  s.route = Route::Hoenn; s.phase = phase; s.nextEncounterId = 1 + encCount;
  if (phase != Phase::Walking) {
    const uint16_t sp = chooseEncounter(s, Route::Hoenn, 7);
    check(sp != 0, "Hoenn species");
    s.encounter.id = 1; s.encounter.species = sp; s.encounter.partner = 1;
    s.encounter.route = Route::Hoenn; s.encounter.seed = 7;
  }
  for (uint8_t i = 0; i < queueCount; ++i) {
    Encounter q; q.id = 2 + i; q.partner = 1; q.route = Route::Hoenn; q.seed = 11 + i;
    q.species = chooseEncounter(s, Route::Hoenn, q.seed);
    check(q.species != 0, "queued species");
    s.encounterQueue[s.encounterQueueSize++] = q;
  }
  if (captureBadges) {
    const int64_t utc = 1792670400LL; const uint32_t day = 20261022u;
    s.clock.calibrated = s.clock.trusted = true; s.clock.dayHighWater = day;
    s.clock.anchor.trusted = true; s.clock.anchor.utcSeconds = utc;
    s.clock.anchor.dayKey = day; s.clock.anchor.bootGeneration = 1;
    s.today.dayKey = day; s.today.steps = 5200; s.today.complete = true; s.bestDaySteps = 5200;
    const uint64_t daily[] = {1000, 3000, 5000}, life[] = {10000, 50000, 100000};
    for (unsigned i = 0; i < 3; ++i) {
      Achievement d; d.reached = true; d.first.timeKnown = true; d.first.dayComplete = true;
      d.first.utcSeconds = utc; d.first.dayKey = day; d.first.bootGeneration = 1;
      d.first.dailySteps = d.first.totalSteps = daily[i];
      s.today.milestones[i] = s.dailyFirst[i] = d;
      s.lifetime[i] = d; s.lifetime[i].first.dailySteps = 5200; s.lifetime[i].first.totalSteps = life[i];
    }
  }
  if (phase == Phase::ThrowReady) s.result = Result::Won;
  if (phase == Phase::Resolved) {
    const uint16_t sp = s.encounter.species;
    const CatalogEntry* entry = catalogEntry(sp);
    const uint8_t prob = captureProbability(kRuleVersion, entry && entry->rare, Timing::Perfect);
    uint8_t roll = 0;
    for (uint32_t seed = 7;; ++seed) {
      s.encounter.seed = seed; roll = throwRoll(s.encounter, 1, kRuleVersion);
      if (roll < prob) break;
    }
    s.result = Result::Captured; s.encounter.usedBalls = 1;
    auto& a = s.encounter.attempts[0];
    a.used = true; a.timing = Timing::Perfect; a.probability = prob; a.roll = roll; a.captured = true;
    setRegistered(s, sp); setOwned(s, sp); s.captureCounts[sp - 1] = 1;
    Capture cap; cap.species = sp; cap.route = Route::Hoenn; cap.at.bootGeneration = 1;
    cap.at.totalSteps = s.totalSteps;
    if (captureBadges) {
      cap.at.timeKnown = true; cap.at.utcSeconds = 1792670400LL; cap.at.dayKey = 20261022u;
      cap.at.dayComplete = true; cap.at.dailySteps = 5200; cap.achievementMask = 63;
    }
    s.firstCapture[sp - 1] = s.recent[0] = cap; s.recentSize = 1;
  }
  check(validate(s) && store.commit(s) == CommitStatus::Committed, "fixture commits");
}
void homeTouchGeometryAndDraws() {
  const menu_layout::Rect captureBox = {302, 66, 92, 30};
  for (i18n::Language lang : {i18n::Language::Chinese, i18n::Language::English}) {
    const auto reg = homeRegionTouchArea(lang);
    check(reg.x + reg.width <= kHomeEvolveTouchArea.x && !rectsOverlap(reg, kHomeEvolveTouchArea),
          "region and evolve disjoint");
    for (uint8_t q : {uint8_t(0), uint8_t(2), uint8_t(11), uint8_t(31)}) {
      MemoryStore store; seedHomeFixture(store, Phase::EncounterReady, q, true, false);
      Engine e(store); check(e.begin(false) == CommandResult::Applied, "load enc");
      const auto enc = homeEncounterTouchArea(lang, e.view());
      check(enc.width > 0 && enc.height > 0 && !rectsOverlap(reg, enc) &&
            !rectsOverlap(kHomeEvolveTouchArea, enc) && !rectsOverlap(captureBox, enc),
            "encounter touch area valid and disjoint");
      ScreenState s; s.page = Page::Home; s.language = lang;
      check(touchHit(s, e, reg.x + reg.width / 2, reg.y + reg.height / 2).aux == PressedAux::HomeRegion &&
            touchHit(s, e, enc.x + enc.width / 2, enc.y + enc.height / 2).aux == PressedAux::HomeEncounter &&
            touchHit(s, e, 233, 82).aux == PressedAux::HomeEvolve &&
            touchHit(s, e, 318, 81).aux == PressedAux::None, "touchHit targets");
    }
    auto checkEmpty = [&](Phase ph, uint8_t q, bool expectActive, const char* msg) {
      MemoryStore st; seedHomeFixture(st, ph, q, false, false);
      Engine en(st); check(en.begin(false) == CommandResult::Applied, msg);
      const auto r = homeEncounterTouchArea(lang, en.view());
      check(expectActive ? r.width > 0 : r.width == 0, msg);
      if (expectActive) {
        ScreenState s; s.page = Page::Home; s.language = lang;
        check(touchHit(s, en, r.x + r.width / 2, r.y + r.height / 2).aux == PressedAux::HomeEncounter, msg);
      }
    };
    checkEmpty(Phase::Walking, 0, false, "walking empty");
    checkEmpty(Phase::ThrowReady, 0, false, "ThrowReady 0-queue empty");
    checkEmpty(Phase::ThrowReady, 2, true, "ThrowReady queued active");
    checkEmpty(Phase::Resolved, 2, false, "Resolved empty");
  }
  {
    MemoryStore store; seedHomeFixture(store, Phase::EncounterReady, 2, true, false);
    Engine e(store); check(e.begin(false) == CommandResult::Applied, "render home zh");
    Screens cached; check(cached.begin(), "init screens");
    ScreenState s; s.page = Page::Home; s.language = i18n::Language::Chinese;
    verifyPartialEq(cached, s, e, 1000, "home-zh-idle", "home-shortcuts-zh");
    s.pressedAux = PressedAux::HomeRegion; verifyPartialEq(cached, s, e, 1020, "home-zh-reg", "home-region-pressed-zh");
    s.pressedAux = PressedAux::None; verifyPartialEq(cached, s, e, 1040, "home-zh-reg-up");
    s.pressedAux = PressedAux::HomeEncounter; verifyPartialEq(cached, s, e, 1060, "home-zh-enc");
    s.pressedAux = PressedAux::HomeEvolve; verifyPartialEq(cached, s, e, 1080, "home-zh-evo");
    s.pressedAux = PressedAux::None; verifyPartialEq(cached, s, e, 1100, "home-zh-evo-up");
  }
  {
    MemoryStore store; seedHomeFixture(store, Phase::EncounterReady, 11, true, false);
    Engine e(store); check(e.begin(false) == CommandResult::Applied, "render home en");
    Screens cached; check(cached.begin(), "init screens en");
    ScreenState s; s.page = Page::Home; s.language = i18n::Language::English;
    verifyPartialEq(cached, s, e, 1000, "home-en-idle", "home-shortcuts-en");
    s.pressedAux = PressedAux::HomeEncounter; verifyPartialEq(cached, s, e, 1020, "home-en-enc", "home-encounter-pressed-en");
    s.pressedAux = PressedAux::None; verifyPartialEq(cached, s, e, 1040, "home-en-enc-up");
  }
  {
    MemoryStore store; seedHomeFixture(store, Phase::Resolved, 5, false, true);
    Engine e(store); check(e.begin(false) == CommandResult::Applied, "render result zh");
    Screens cached; check(cached.begin(), "init result");
    ScreenState s; s.page = Page::Result; s.language = i18n::Language::Chinese;
    verifyPartialEq(cached, s, e, 1000, "result-zh-full", "result-captured-full-badges-queue-zh");
    check(rendered_text::contains("捕捉成功", theme::kText) &&
          rendered_text::contains("首次捕获", theme::kGold) &&
          rendered_text::contains("每日：1千 3千 5千步", theme::kGold) &&
          rendered_text::contains("累计：1万 5万 10万步", theme::kGold) &&
          rendered_text::contains("下一场遭遇 (5)", theme::kGold), "zh result text");
    s.pressedIndex = 0; verifyPartialEq(cached, s, e, 1020, "result-zh-row");
    s.pressedIndex = -1; s.pressedAux = PressedAux::Back; verifyPartialEq(cached, s, e, 1040, "result-zh-back");
    s.pressedAux = PressedAux::None; verifyPartialEq(cached, s, e, 1060, "result-zh-up");

    MemoryStore enStore; seedHomeFixture(enStore, Phase::Resolved, 12, false, true);
    Engine en(enStore); check(en.begin(false) == CommandResult::Applied, "render result en");
    s.language = i18n::Language::English;
    verifyPartialEq(cached, s, en, 1000, "result-en-full", "result-captured-full-badges-queue-en");
    check(rendered_text::contains("Caught!", theme::kText) &&
          rendered_text::contains("First catch", theme::kGold) &&
          rendered_text::contains("Daily: 1k 3k 5k steps", theme::kGold) &&
          rendered_text::contains("Total: 10k 50k 100k steps", theme::kGold) &&
          rendered_text::contains("Next encounter (12)", theme::kGold), "en result text");
  }
}
void dragCancelHome(Runtime& r, TestPlatform& p, int sx, int sy, PressedAux expected, const char* what) {
  p.next.touch = true; p.next.touchDown = true; p.next.held = true; p.next.x = sx; p.next.y = sy; frame(r);
  check(r.screen().pressedAux == expected, what);
  p.next.touchDown = true; p.next.held = true; p.next.x = 233; p.next.y = 200; frame(r);
  p.next.x = 233; p.next.y = 200; frame(r);
  check(r.screen().page == Page::Home && r.screen().pressedAux == PressedAux::None, what);
}
void homeAndResultRuntimePaths() {
  MemoryStore store; seedHomeFixture(store, Phase::EncounterReady, 2, true, false);
  TestPlatform p; Runtime r(store, p); start(r);
  const auto reg = homeRegionTouchArea(r.language()), enc = homeEncounterTouchArea(r.language(), r.engine().view());
  dragCancelHome(r, p, reg.x + reg.width / 2, reg.y + reg.height / 2, PressedAux::HomeRegion, "drag-out HomeRegion");
  dragCancelHome(r, p, enc.x + enc.width / 2, enc.y + enc.height / 2, PressedAux::HomeEncounter, "drag-out HomeEncounter");

  tap(r, p, reg.x + reg.width / 2, reg.y + reg.height / 2);
  check(r.screen().page == Page::Routes, "region pill opens Routes");
  tap(r, p, 233, 35);
  check(r.screen().page == Page::Home, "top back from Home-opened Routes returns Home");
  tap(r, p, reg.x + reg.width / 2, reg.y + reg.height / 2);
  select(r, p, kRegionCount);
  check(r.screen().page == Page::Home, "Back row from Home-opened Routes returns Home");

  openMenu(r, p); select(r, p, MenuItem::Region);
  check(r.screen().page == Page::Routes, "menu Region opens Routes");
  tap(r, p, 233, 35);
  check(r.screen().page == Page::Menu && r.screen().selected == unsigned(MenuItem::Region),
        "back from menu-opened Routes restores Menu Region focus");
  tap(r, p, 233, 35);
  check(r.screen().page == Page::Home, "back from menu returns Home");

  tap(r, p, enc.x + enc.width / 2, enc.y + enc.height / 2);
  check(r.screen().page == Page::Encounter, "encounter badge opens Encounter");
  select(r, p, 3);
  check(r.screen().page == Page::Home, "BattleLater returns Home");

  {
    MemoryStore resStore; seedHomeFixture(resStore, Phase::Resolved, 2, false, false);
    TestPlatform rp; Runtime rr(resStore, rp); start(rr);
    check(rr.screen().page == Page::Result && rr.engine().view().encounter.id == 1, "starts on Result");
    resStore.fail = true;
    tap(rr, rp, 233, 35); frame(rr);
    check(rr.screen().page == Page::Error && rr.engine().savePending() && rr.engine().view().phase == Phase::Resolved,
          "failed Result top-back enters Error");
    resStore.fail = false; select(rr, rp, 0);
    check(rr.screen().page == Page::Home && rr.engine().view().encounter.id == 2 &&
          rr.engine().view().phase == Phase::EncounterReady, "retry promotes next encounter");
  }
  {
    MemoryStore resStore; seedHomeFixture(resStore, Phase::Resolved, 0, false, false);
    TestPlatform rp; Runtime rr(resStore, rp); start(rr);
    check(rr.screen().page == Page::Result && rendered_text::contains("确认结果 · 继续探索", theme::kGold),
          "empty queue Result shows AcceptResult");
    tap(rr, rp, 233, 35);
    check(rr.screen().page == Page::Home && rr.engine().view().phase == Phase::Walking,
          "top back on empty queue Result returns Home");
  }
}
}

int main() {
  c152::begin("ADVENTURE TEST");
  clockAndErrors(); midnightAndSampling(); slowGaitAttribution(); stallAttribution(); practiceMidnightAchievement();
  homeTouchGeometryAndDraws(); homeAndResultRuntimePaths();
  std::printf("%u adventure Runtime checks, 0 failures\n", checks);
}
