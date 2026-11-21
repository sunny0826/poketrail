#include "support/progression_ui.hpp"
#include "battle_presentation.hpp"
#include "support/rendered_text.hpp"
#include <zlib.h>
using namespace progression_ui;
namespace {
class DegradedStore : public MemoryStore {
 public:
  LoadResult load(Save& save) override {
    auto result = MemoryStore::load(save);
    if (result.status == LoadStatus::Loaded) result.degraded = true;
    return result;
  }
};
void longLabelsAndWarnings() {
  MemoryStore store; legacyOwned(store, {1, 122, 138, 139}, 122);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); openMenu(runtime);
  select(runtime, MenuItem::Partner); select(runtime, 0);
  edge(runtime, false, true); screenshot(runtime, "long-partner-carried");
  tap(runtime, 233, 374); tap(runtime, 233, 238);
  check(runtime.engine().view().partner == 138, "long-name fixture carries Omanyte");
  select(runtime, 1);
  check(rendered_text::contains("已拥有此形态", theme::kGold), "owned evolution target has a readable refusal");
  screenshot(runtime, "long-evolution-owned");
  MemoryStore unready; legacyOwned(unready, {1, 138}, 138);
  Runtime growing(unready, platform); start(growing); openMenu(growing); select(growing, MenuItem::Partner); select(growing, 1);
  check(rendered_text::contains("培养步数还不够", theme::kGold), "unavailable evolution explains the missing progress");
  screenshot(growing, "long-evolution-unready");
  DegradedStore backup; legacyOwned(backup, {1});
  Runtime recovered(backup, platform); start(recovered);
  check(rendered_text::contains("已从备用存档恢复", theme::kGold), "recovery warning is readable on exploration");
  screenshot(recovered, "home-recovery-warning");
  Save saved; check(store.load(saved).status == LoadStatus::Loaded, "step-warning fixture loads saved state");
  saved.stepsIncomplete = true; saved.today.complete = false;
  check(store.commit(saved) == CommitStatus::Committed, "step-warning fixture validates");
  Runtime incomplete(store, platform); start(incomplete);
  check(rendered_text::contains("部分步数缺失", theme::kRed), "step warning is readable on exploration");
  screenshot(incomplete, "home-step-warning");
}
}
void mainMenuHub() {
  MemoryStore walking; legacyOwned(walking, {1, 4});
  HostPlatform platform; Runtime runtime(walking, platform); start(runtime); openMenu(runtime);
  screenshot(runtime, "menu-hub-walking");
  edge(runtime, true, false);
  check(runtime.screen().selected == unsigned(MenuItem::Explore), "left from the first tile wraps past the walking banner");
  edge(runtime, false, true);
  check(runtime.screen().selected == unsigned(MenuItem::Region), "right from the exit wraps past the walking banner");
  tap(runtime, 233, 104);
  check(runtime.screen().page == Page::Menu, "walking banner ignores touch");
  tap(runtime, 107, 296);
  check(runtime.screen().page == Page::Records && runtime.screen().selected == 0, "records tile opens the records hub");
  screenshot(runtime, "records-hub");
  tap(runtime, 233, 389);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == unsigned(MenuItem::Records),
        "records exit pill returns focus to its tile");
  tap(runtime, 359, 296);
  check(runtime.screen().page == Page::Settings, "settings tile opens settings");
  screenshot(runtime, "settings-page");

  MemoryStore waiting;
  { Engine e(waiting); e.begin(); e.chooseStarter(1); step(e, 1, 5000); e.checkpoint(); }
  Runtime encounter(waiting, platform); start(encounter); openMenu(encounter);
  check(rendered_text::contains("继续遭遇", theme::kGold, 78, 130), "waiting encounter banner names the resume action");
  screenshot(encounter, "menu-hub-encounter");
  edge(encounter, true, false);
  check(encounter.screen().selected == unsigned(MenuItem::Explore), "left from a waiting banner reaches the exit");
  edge(encounter, false, true);
  check(encounter.screen().selected == unsigned(MenuItem::Encounter), "waiting banner joins key navigation");
  tap(encounter, 233, 104);
  check(encounter.screen().page == Page::Encounter, "touching the waiting banner opens the encounter");
}
namespace {
void touchRelease(Runtime& runtime, int startX, int startY, int endX, int endY, bool clicked = true) {
  auto& detail = M5.Touch.detail;
  detail.base_x = detail.x = startX; detail.base_y = detail.y = startY; detail.pressed = true;
  frame(runtime);
  detail.x = endX; detail.y = endY; detail.pressed = false; detail.released = true; detail.clicked = clicked;
  frame(runtime);
  detail = HostTouchDetail();
}
void touchToleranceAndHomeEvolve() {
  MemoryStore walking; legacyOwned(walking, {1, 4});
  HostPlatform platform; Runtime menu(walking, platform); start(menu); openMenu(menu);
  touchRelease(menu, 359, 296, 375, 296);
  check(menu.screen().page == Page::Settings, "16px micro-jitter still activates a menu tile");
  touchRelease(menu, 233, 35, 221, 45);
  check(menu.screen().page == Page::Menu, "jitter within tolerance still activates back");
  touchRelease(menu, 359, 296, 384, 296, false);
  check(menu.screen().page == Page::Menu, "25px short drag does not activate a menu tile");

  MemoryStore evolvable;
  { Engine e(evolvable); e.begin(); e.chooseStarter(1); step(e, 1, 7500); e.checkpoint(); }
  Runtime home(evolvable, platform); start(home);
  check(home.screen().page == Page::Home && homeEvolutionReady(home.engine()),
        "home evolve fixture starts ready on exploration");
  screenshot(home, "home-evolve-ready");

  auto& detail = M5.Touch.detail;
  detail.base_x = detail.x = 233; detail.base_y = detail.y = 76; detail.pressed = true;
  frame(home);
  check(home.screen().page == Page::Home,
        "pressing the home evolve button waits for release instead of opening the menu");
  detail.x = 249; detail.y = 76; detail.pressed = false; detail.released = detail.clicked = true;
  frame(home);
  detail = HostTouchDetail();
  check(home.screen().page == Page::EvolutionConfirm &&
        home.screen().evolution.from == 1 && home.screen().evolution.to == 2,
        "16px micro-jitter click on the home evolve button opens evolution confirm without triggering back");
  screenshot(home, "home-evolve-confirm");

  tap(home, 233, 35);
  check(home.screen().page == Page::Home, "back returns from evolution confirm to home");
  touchRelease(home, 169, 62, 176, 61);
  check(home.screen().page == Page::EvolutionConfirm,
        "8px expanded evolve touch padding above y=60 opens evolution confirm");
  tap(home, 233, 35);
  check(home.screen().page == Page::Home, "back returns to home after top-edge evolve tap");
  touchRelease(home, 297, 102, 286, 98);
  check(home.screen().page == Page::EvolutionConfirm,
        "8px expanded evolve touch padding at bottom-right opens evolution confirm");
  tap(home, 233, 35);
  touchRelease(home, 233, 59, 233, 59);
  check(home.screen().page == Page::Menu,
        "touch above y=60 stays outside evolve hitbox and opens the menu");
  tap(home, 233, 35);
  check(home.screen().page == Page::Home, "menu back returns to home");
  touchRelease(home, 233, 82, 283, 82, false);
  check(home.screen().page == Page::Home,
        "horizontal swipe across the home evolve button does not trigger evolution or menu");
  touchRelease(home, 233, 82, 233, 82, false);
  check(home.screen().page == Page::Home,
        "hold release on the home evolve button does not trigger evolution or menu");
}
void savePng(const char* name) {
  constexpr int N = 468; std::vector<uint8_t> raw(N * (1 + N * 3), 0);
  for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
    uint16_t p = M5.Display.readPixel(x, y); uint8_t* r = &raw[y * (1 + N * 3) + 1 + x * 3];
    r[0] = (p >> 11) * 255 / 31; r[1] = ((p >> 5) & 63) * 255 / 63; r[2] = (p & 31) * 255 / 31;
  }
  uLongf zlen = compressBound(uLong(raw.size())); std::vector<uint8_t> z(zlen);
  check(compress2(z.data(), &zlen, raw.data(), uLong(raw.size()), Z_BEST_SPEED) == Z_OK, "png");
  FILE* f = std::fopen(verificationOutput((std::string(name) + ".png").c_str()).c_str(), "wb"); check(f != nullptr, "open");
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
void verifyPartialEq(Screens& c, const ScreenState& s, const Engine& e,
                     uint32_t now, const char* lbl, Screens* sr = nullptr, const char* png = nullptr) {
  check(c.draw(s, e, {}, now), lbl); const auto got = M5.Display.framebuffer();
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x)
    if ((x-233)*(x-233)+(y-233)*(y-233) > 228*228 && M5.Display.readPixel(x, y) != TFT_BLACK) check(false, lbl);
  if (png) savePng(png);
  if (sr) {
    ScreenState f = s; f.page = s.page == Page::Settings ? Page::Pause : Page::Settings;
    check(sr->draw(f, e, {}, now) && sr->draw(s, e, {}, now), lbl);
  } else { Screens fresh; check(fresh.begin() && fresh.draw(s, e, {}, now), lbl); }
  const auto& ref = M5.Display.framebuffer();
  if (got != ref) {
    for (size_t i = 0; i < got.size(); ++i) if (got[i] != ref[i]) {
      std::fprintf(stderr, "mismatch %s (%zu,%zu) t=%u\n", lbl, i%468, i/468, now); break;
    }
    check(false, lbl);
  }
}
void testPartialEquivalence() {
  constexpr auto kOk = CommandResult::Applied;
  MemoryStore store; ScreenState s; Screens c, ref; check(c.begin() && ref.begin(), "eq scr");
  {
    Engine e(store); check(e.begin() == kOk && e.chooseStarter(1) == kOk, "eq0");
    auto v = [&](uint32_t t, Screens* r = nullptr, const char* p = nullptr) { verifyPartialEq(c, s, e, t, "home eq", r, p); };
    s.page = Page::Home; v(1000, &ref);
    c.advanceHome(1, 1300); ref.advanceHome(1, 1300); v(1300, &ref);
    step(e, 1, 500);
    for (uint32_t t : {1400u, 1500u, 1600u}) { c.advanceScene(Route::Kanto, t, true); ref.advanceScene(Route::Kanto, t, true); }
    v(1600, &ref, "home-partial-updated");
    c.advanceHome(1, 1900); ref.advanceHome(1, 1900); v(1900, &ref);
    step(e, 1, 7000); Save sv = e.view();
    setOwned(sv, 133); setRegistered(sv, 133); setOwned(sv, 4); setRegistered(sv, 4); setRegistered(sv, 25);
    sv.encounter.species = 25; sv.encounter.seed = 7;
    sv.captureCounts[0] = 1; sv.recentSize = 1; sv.recent[0].species = 1; sv.recent[0].route = Route::Kanto;
    sv.recent[0].at.bootGeneration = 1; sv.recent[0].at.totalSteps = 500; sv.firstCapture[0] = sv.recent[0];
    check(validate(sv) && store.commit(sv) == CommitStatus::Committed, "eq save");
  }
  Engine e(store); check(e.begin() == kOk, "eq1");
  auto v = [&](uint32_t t, const char* l = "eq", Screens* r = nullptr, const char* p = nullptr) { verifyPartialEq(c, s, e, t, l, r, p); };
  auto seq = [&](Page p, std::initializer_list<uint16_t> sels, Screens* r = nullptr, uint32_t dt = 0) {
    s.page = p; uint32_t t = 1000; for (uint16_t sel : sels) { s.selected = sel; v(t, "list eq", r); t += dt; }
  };
  s.battery.percent = 85; s.battery.charge = c152::ChargeState::Discharging; s.page = Page::Menu;
  for (uint16_t sel : {0,1,2,3,4,6,7,0,7,1}) {
    s.selected = sel; if (sel == 2) { s.battery.percent = 15; s.battery.charge = c152::ChargeState::Charging; }
    v(1000, "menu", nullptr, sel == 7 ? "menu-partial-exit" : sel == 0 ? "menu-partial-banner" : nullptr);
  }
  seq(Page::Records, {0,1,2,3,4,0,4});
  s.page = Page::Routes;
  for (uint16_t sel : {0,1,2,3,4,5,0}) { s.selected = sel; v(1000, "routes", nullptr, sel == 4 ? "routes-partial-unova" : nullptr); }
  seq(Page::Partners, {0,1,2,3,0}); seq(Page::CarriedPartner, {0,1,2,3,0});
  seq(Page::Settings, {0,1,2,3,4,0}); seq(Page::Starter, {0,1,2,3,0,1}, &ref, 200);
  s.species = 133; seq(Page::PartnerDetail, {0,1,2,3,4,1,0});
  seq(Page::Clock, {0,1,3,6,8,3});
  s.editing = true; s.civil.hour = 15; v(1000, "clke", nullptr, "clock-partial-edit");
  s.clockError = true; s.editing = false; v(1000, "clkx"); s.clockError = false; v(1000, "clko");
  s.page = Page::Language; s.selected = 0; v(1000); s.selected = 1; s.language = i18n::Language::English; v(1000);
  s.language = i18n::Language::Chinese; s.selected = 2; v(1000);
  s.page = Page::TestFaults; s.testMode = true;
  for (uint16_t sel : {0,1,3,4,0}) { s.selected = sel; s.testFault = TestFault(sel < 4 ? sel : 0); v(1000, "tf"); }
  s.testMode = false;
  for (Page rp : {Page::FirstCaptures, Page::Days, Page::Achievements})
    seq(rp, {0, 1, uint16_t(menuCount((s.page = rp, s), e) - 1)});

  uint32_t t = 1000; check(e.startBattle(t) == kOk, "bstart");
  auto vb = [&](uint32_t tm, const char* l, const char* p = nullptr) { verifyPartialEq(c, s, e, tm, l, &ref, p); };
  s = ScreenState(); s.page = Page::Battle; vb(t, "bcd3");
  bool cd1 = false, miss = false, dodge = false, shake = false, post = false;
  for (int g = 0; g < 1200 && e.battleView().active; ++g) {
    t += 30; e.tick(t, {}); const BattleView& bv = e.battleView();
    if (!cd1 && countdownDigit(bv, t) == 1) { cd1 = true; vb(t, "bcd1"); }
    if (!bv.consumed && !countdownDigit(bv, t) && int32_t(bv.targetMs - t) <= 15) {
      if (!miss && bv.expected == Action::Dodge) miss = true; else e.press(bv.expected, t);
    }
    if (miss && !shake && bv.playerHp < kPlayerHp) vb(t, "bhp");
    if (!dodge && bv.settledCue == Action::Dodge && bv.lastTiming != Timing::Miss && t - bv.settledMs < 150) { dodge = true; vb(t, "bdg"); }
    if (!shake && bv.lastTiming == Timing::Perfect && bv.settledCue == Action::Attack && t - bv.settledMs >= 100 && t - bv.settledMs < 150) {
      shake = true; vb(t, "bsh", "battle-partial-shake");
    } else if (shake && !post && !battleEffectActive(bv, t)) { post = true; vb(t, "bpst", "battle-partial-post"); }
    else if (g % 15 == 0) c.draw(s, e, {}, t);
  }
  check(cd1 && miss && dodge && shake && post && e.view().phase == Phase::ThrowReady, "bbeat");
  s.page = Page::Throw; vb(t, "ti"); t += 260; vb(t, "ta");
  const auto key = AttemptKey(e.view().encounter.id, e.view().encounter.usedBalls + 1);
  check(e.beginThrow(key, t) == kOk, "bth");
  t += 500; vb(t, "tai"); s.throwCommitting = true; vb(t, "tc"); s.throwCommitting = false;
  ClockReading clk; clk.bootGeneration = e.view().bootGeneration;
  check(e.pressThrow(key, t, clk) == kOk, "pth");
  s.capture.active = true; s.capture.startMs = t; s.capture.timing = Timing::Nice; s.capture.wiggleBreak = 1;
  for (uint32_t dt : {200u,500u,800u,1100u,1600u}) vb(t + dt, "tbr", dt == 1600u ? "throw-partial-breakfree" : nullptr);
  s.capture = CaptureAnim(); vb(t + 2200, "trs");
  s.capture.active = true; s.capture.startMs = t + 2300; s.capture.timing = Timing::Perfect; s.capture.captured = true;
  vb(t + 5000, "tcp", "throw-partial-captured");
  s = ScreenState(); s.page = Page::Evolution; s.evolutionAnim.from = 1; s.evolutionAnim.to = 2; s.evolutionAnim.startMs = 1000;
  for (uint32_t off : {100u,950u,2900u,3300u})
    verifyPartialEq(c, s, e, 1000 + off, "evo", nullptr, off == 3300u ? "evolution-partial-reveal" : nullptr);
}
}
int main() {
  c152::begin("MENU OUTPUT TEST"); longLabelsAndWarnings(); mainMenuHub(); touchToleranceAndHomeEvolve();
  testPartialEquivalence();
  std::printf("%u menu output checks, 0 failures\n", checks);
}
