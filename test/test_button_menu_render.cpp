#include "support/progression_ui.hpp"
#include "support/rendered_text.hpp"
#include <zlib.h>
using namespace progression_ui;
namespace {
void release(Runtime& r) { buttons(false, false); frame(r); }
void chord(Runtime& r, bool leftFirst, unsigned delay = 80) {
  buttons(leftFirst, !leftFirst); frame(r); idle(r, delay);
  buttons(true, true); frame(r);
}
void staggeredConfirmation(bool leftFirst, unsigned delay) {
  MemoryStore store; HostPlatform platform; Runtime r(store, platform); start(r);
  edge(r, false, true);
  check(r.screen().page == Page::DateWelcome && r.screen().selected == 1,
        "single key selects skip date before confirmation");
  buttons(leftFirst, !leftFirst); frame(r); idle(r, delay);
  check(r.screen().selected == 1, "first held key does not move the intended selection");
  buttons(true, true); frame(r); idle(r, 1000);
  check(r.screen().page == Page::Starter && r.engine().view().starter == 0,
        leftFirst ? "left then right confirms original skip-date selection" :
                    "right then left confirms original skip-date selection");
  buttons(!leftFirst, leftFirst); frame(r); idle(r, 80);
  buttons(true, true); frame(r);
  check(!r.engine().view().starter && r.screen().selected == 0,
        "partial release and re-press cannot confirm on the new page");
  release(r); chord(r, !leftFirst); release(r);
  check(r.engine().view().starter == 1 && r.screen().page == Page::Home,
        "fresh staggered chord after full release chooses starter once");
}
void singleAndNonOverlapping() {
  MemoryStore store; HostPlatform platform; Runtime r(store, platform); start(r);
  buttons(false, true); frame(r); idle(r, 1000);
  check(r.screen().selected == 0, "held single key waits for release without repeats");
  release(r); idle(r, 1000);
  check(r.screen().selected == 1, "single key release moves exactly once");
  buttons(true, false); frame(r);
  buttons(false, true); frame(r);
  check(r.screen().selected == 0 && r.screen().page == Page::DateWelcome,
        "non-overlapping switch between keys is not a chord");
  release(r);
  check(r.screen().selected == 1, "second non-overlapping key releases as its own action");
  buttons(true, false); frame(r); idle(r, 30100);
  check(!r.dimmed() && r.screen().selected == 1, "holding a key keeps menu awake without movement");
  buttons(true, true); frame(r);
  check(r.screen().page == Page::Starter, "slow overlapping chord has no arbitrary timing cutoff");
  release(r);
}
void clockAndSamePage() {
  MemoryStore store; HostPlatform platform; Runtime r(store, platform); start(r);
  select(r, 0); check(r.screen().page == Page::Clock, "clock is reachable");
  const int year = r.screen().civil.year;
  chord(r, true); idle(r, 1000);
  check(r.screen().editing && r.screen().civil.year == year, "held confirmation enters editing only once");
  release(r);
  buttons(false, true); frame(r); idle(r, 80);
  check(r.screen().civil.year == year, "clock value waits for single key release");
  release(r); check(r.screen().civil.year == year + 1, "clock single release changes value once");
  chord(r, true); idle(r, 1000);
  check(!r.screen().editing && r.screen().civil.year == year + 1,
        "staggered chord exits editing without changing value or re-entering");
  buttons(false, true); frame(r); buttons(true, true); frame(r);
  check(!r.screen().editing && r.screen().selected == 0, "same-page chord remains consumed until both release");
  release(r); edge(r, false, true);
  check(r.screen().selected == 1, "new single key works after same-page confirmation");
}
void touchCancelsPending() {
  MemoryStore store; HostPlatform platform; Runtime r(store, platform); start(r);
  edge(r, false, true);
  buttons(false, true); frame(r); tap(r, 20, 276);
  buttons(true, true); frame(r); release(r);
  check(r.screen().page == Page::DateWelcome && r.screen().selected == 1,
        "blank touch consumes a pending key until all keys release");
  buttons(false, true); frame(r);
  auto& touch = M5.Touch.detail;
  buttons(false, false); touch.x = 20; touch.y = 276; touch.pressed = true; frame(r);
  touch = HostTouchDetail(); frame(r);
  check(r.screen().selected == 1, "touch on release frame takes precedence over pending single key");
  buttons(true, false); frame(r); tap(r, 233, 276); release(r);
  check(r.screen().page == Page::Starter && r.screen().selected == 0,
        "valid click release acts once and cancels key navigation on next page");
  select(r, 3); select(r, 0); select(r, 0);
  buttons(false, true); frame(r); tap(r, 233, 380); release(r);
  check(!r.screen().editing && r.screen().selected == 0,
        "touch ending clock editing cancels pending key without moving field");
  buttons(false, true); frame(r); tap(r, 298, 430); release(r);
  check(r.screen().selected == 3, "touch pagination cancels a pending key on same page");
}
void wakeAndRecords() {
  MemoryStore store; HostPlatform platform; Runtime r(store, platform); start(r);
  edge(r, false, true); idle(r, 30100); check(r.dimmed(), "inactive menu sleeps");
  chord(r, true); idle(r, 1000);
  check(r.dimmed(), "staggered chord cannot wake the locked screen");
  M5.powerClick(); frame(r); idle(r, 160);
  check(!r.dimmed() && r.screen().page == Page::DateWelcome && r.screen().selected == 1,
        "power wakes without executing the held staggered chord");
  release(r); chord(r, false); release(r); select(r, 0);
  openMenu(r); select(r, MenuItem::Records); select(r, 1);
  check(r.screen().page == Page::Recent && menuCount(r.screen(), r.engine()) == 1,
        "empty recent records still have a return item");
  chord(r, true); release(r); check(r.screen().page == Page::Records, "empty record return accepts staggered chord");
  select(r, 0); chord(r, false); idle(r, 1000); release(r);
  check(r.screen().page == Page::FirstCaptures && r.screen().selected == 0,
        "record content confirmation stays on current entry");
  edge(r, true, false); chord(r, true); release(r);
  check(r.screen().page == Page::Records, "record return after single-key wrap accepts chord");
}
void retryOnce() {
  MemoryStore store; HostPlatform platform; Runtime r(store, platform); start(r);
  select(r, 1); store.fail = true; chord(r, true); release(r);
  check(r.screen().page == Page::Error && r.engine().savePending() && !r.engine().view().starter,
        "failed starter commit shows no reward");
  chord(r, false); idle(r, 1000);
  store.fail = false;
  idle(r, 1000);
  buttons(true, false); frame(r); buttons(true, true); frame(r);
  check(r.screen().page == Page::Error && r.engine().savePending() && !r.engine().view().starter,
        "held or partly released retry cannot publish rewards after storage recovers");
  release(r); chord(r, true); release(r);
  check(r.engine().view().starter == 1 && r.screen().page == Page::Home && !r.engine().savePending(),
        "new retry chord publishes the same pending starter");
}
void seedEncounter(MemoryStore& store) {
  Engine e(store); check(e.begin() == CommandResult::Applied, "encounter fixture boots");
  e.chooseStarter(1); step(e, 1, 5000); e.checkpoint();
}
BattleView battleView(const Runtime& r, bool practice) { return practice ? r.practiceView() : r.engine().battleView(); }
void pauseAndResume(bool practice) {
  MemoryStore store; seedEncounter(store); HostPlatform platform; Runtime r(store, platform); start(r);
  openMenu(r);
  if (practice) select(r, MenuItem::Practice); else { select(r, MenuItem::Encounter); select(r, 0); }
  const Page active = practice ? Page::Practice : Page::Battle;
  check(r.screen().page == active && !battleView(r, practice).consumed,
        "entering battle consumes held menu confirmation without attacking");
  auto view = battleView(r, practice);
  buttons(false, true); frame(r, view.targetMs - host_time_ms);
  check(battleView(r, practice).consumed && battleView(r, practice).lastTiming == Timing::Perfect,
        "battle action is scored on press time, before release");
  release(r);
  buttons(true, true); frame(r); frame(r, 799);
  check(!battleView(r, practice).pauseRequested && r.screen().page == active, "799ms overlap does not request pause");
  frame(r, 1); check(battleView(r, practice).pauseRequested, "800ms overlap requests pause");
  for (unsigned i = 0; i < 100 && r.screen().page != Page::Pause; ++i) frame(r);
  check(r.screen().page == Page::Pause, "hold pause waits for current beat to close");
  idle(r, 1000); check(r.screen().page == Page::Pause, "held pause chord cannot auto-resume");
  release(r); chord(r, false); idle(r, 80);
  check(r.screen().page == active && !battleView(r, practice).consumed,
        "staggered resume keeps new beat free from residual edges");
  release(r); buttons(true, true); frame(r); idle(r, kOpeningMs + kNormalWindowMs + 200);
  check(r.screen().page == Page::Pause, "fresh long chord can pause again");
  release(r); edge(r, false, true); chord(r, true); release(r);
  check(r.screen().page == (practice ? Page::Home : Page::Result), "staggered retreat confirms the selected pause item");
}

void clearTouch(Runtime& r) { M5.Touch.detail = HostTouchDetail(); frame(r); }
void touchDown(Runtime& r, int x, int y) {
  auto& t = M5.Touch.detail; t.base_x = t.x = x; t.base_y = t.y = y; t.pressed = true; frame(r);
}
void touchLift(Runtime& r, int x, int y) {
  auto& t = M5.Touch.detail; t.x = x; t.y = y; t.pressed = false; t.released = t.clicked = true; frame(r); clearTouch(r);
}
void hold(Runtime& r, bool l, bool v, unsigned ms) { buttons(l, v); frame(r); idle(r, ms); }

void savePng(const char* name) {
  constexpr int N = 468; std::vector<uint8_t> raw(N * (1 + N * 3), 0);
  for (int i = 0; i < N * N; ++i) {
    const uint16_t p = M5.Display.readPixel(i % N, i / N); uint8_t* q = &raw[(i / N) * (1 + N * 3) + 1 + (i % N) * 3];
    q[0] = (p >> 11) * 255 / 31; q[1] = ((p >> 5) & 63) * 255 / 63; q[2] = (p & 31) * 255 / 31;
  }
  uLongf zl = compressBound(uLong(raw.size())); std::vector<uint8_t> z(zl);
  check(compress2(z.data(), &zl, raw.data(), uLong(raw.size()), Z_BEST_SPEED) == Z_OK, "png");
  FILE* f = std::fopen(verificationOutput((std::string(name) + ".png").c_str()).c_str(), "wb"); check(f, "png");
  auto w = [&](const char* t, const uint8_t* d, size_t n) {
    uint8_t b[8] = {uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)}; std::fwrite(b,1,4,f); std::fwrite(t,1,4,f);
    if (n) std::fwrite(d,1,n,f); uint32_t c = uint32_t(crc32(0, reinterpret_cast<const Bytef*>(t), 4));
    if (n) c = uint32_t(crc32(c, d, uInt(n)));
    b[0]=c>>24; b[1]=c>>16; b[2]=c>>8; b[3]=c; std::fwrite(b,1,4,f);
  };
  static const uint8_t hdr[13] = {0,0,1,212,0,0,1,212,8,2,0,0,0};
  std::fwrite("\x89PNG\r\n\x1a\n", 1, 8, f); w("IHDR", hdr, 13); w("IDAT", z.data(), zl); w("IEND", nullptr, 0); std::fclose(f);
}

void checkEq(const Runtime& r, const char* lbl, bool png = false) {
  for (int i = 0; i < 468 * 468; ++i) {
    const int dx = i % 468 - 233, dy = i / 468 - 233;
    if (dx * dx + dy * dy > 228 * 228 && M5.Display.readPixel(i % 468, i / 468) != TFT_BLACK) check(false, lbl);
  }
  const auto got = M5.Display.framebuffer(); if (png) savePng(lbl);
  Screens fresh; check(fresh.begin() && fresh.draw(r.screen(), r.engine(), r.practiceView(), host_time_ms) && got == M5.Display.framebuffer(), lbl);
}

void checkHit(Runtime& r, int x, int y, int16_t idx, const char* png = nullptr, PressedAux aux = PressedAux::None) {
  const char* lbl = png ? png : "hit";
  touchDown(r, x, y); check(r.screen().pressedIndex == idx && r.screen().pressedAux == aux, lbl);
  checkEq(r, lbl, png != nullptr); clearTouch(r);
}

void checkDrag(Runtime& r, int sx, int sy, int dx, int dy, int rx, int ry) {
  const Page pg = r.screen().page; const uint16_t sel = r.screen().selected;
  touchDown(r, sx, sy); M5.Touch.detail.x = dx; M5.Touch.detail.y = dy; frame(r);
  check(r.screen().pressedIndex == -1 && r.screen().pressedAux == PressedAux::None, "d0");
  checkEq(r, "d1"); touchLift(r, rx, ry); check(r.screen().page == pg && r.screen().selected == sel, "d2");
}

void checkNoRepeat(Runtime& r) {
  const uint16_t s0 = r.screen().selected;
  hold(r, false, true, 1000); check(r.screen().selected == s0, "nr");
  release(r); check(r.screen().selected == s0 + 1, "s1");
}

void repeatAndPressedSuite() {
  MemoryStore sClock, sSmall, sLarge, sActive;
  legacyOwned(sSmall, {1, 4}); legacyOwned(sLarge, {1, 4, 25, 133, 138, 139});
  { Engine e(sActive); e.begin(); e.chooseStarter(1); step(e, 1, 7500); e.checkpoint(); }
  HostPlatform platform;
  {
    Runtime r(sClock, platform); start(r); select(r, 0); checkNoRepeat(r);
    select(r, 4); int m = r.screen().civil.minute;
    hold(r, false, true, 300); check(r.screen().civil.minute == m, "c0");
    idle(r, 1200); check(r.screen().civil.minute > m + 8, "c1"); m = r.screen().civil.minute;
    release(r); check(r.screen().civil.minute == m, "c2");
    hold(r, false, true, 400); check(r.screen().civil.minute > m, "c3"); m = r.screen().civil.minute;
    hold(r, true, true, 200); release(r); check(r.screen().editing && r.screen().civil.minute == m, "c4");
  }
  {
    Runtime small(sSmall, platform); start(small); openMenu(small); select(small, MenuItem::Partner); select(small, 0);
    checkNoRepeat(small);
  }
  {
    Runtime r(sActive, platform); start(r); checkEq(r, "home-evolve-normal", true);
    touchDown(r, 233, 80); check(r.screen().pressedAux == PressedAux::HomeEvolve && r.screen().page == Page::Home, "e0");
    checkEq(r, "home-evolve-pressed", true);
    M5.Touch.detail.y = 55; frame(r); check(r.screen().pressedAux == PressedAux::None, "e1"); checkEq(r, "e2");
    touchLift(r, 233, 80); check(r.screen().page == Page::Home, "e3");

    openMenu(r); touchDown(r, 233, 104); check(r.screen().pressedIndex == 0, "b0"); checkEq(r, "menu-banner-pressed", true);
    touchLift(r, 233, 104); check(r.screen().page == Page::Encounter, "b1");
    checkHit(r, 233, 306, 1, "encounter-row-pressed");
    select(r, 3); openMenu(r); select(r, MenuItem::Pokedex); check(viewer_screen::species() == 1, "v1");
    const auto fb = M5.Display.framebuffer();
    touchDown(r, 233, 423); check(M5.Display.framebuffer() != fb, "vp0"); savePng("viewer-preview-pressed");
    M5.Touch.detail.x = 140; frame(r); check(M5.Display.framebuffer() == fb, "vp1");
    touchLift(r, 233, 423); check(r.screen().page == Page::Viewer, "vp2");
  }
  {
    Runtime r(sLarge, platform); start(r); openMenu(r);
    checkEq(r, "menu-normal", true); checkHit(r, 233, 104, -1);
    checkHit(r, 233, 192, 2, "menu-tile-pressed"); check(rendered_text::contains("伙伴", theme::kText), "t");
    checkHit(r, 107, 192, 1, "menu-active-pressed"); checkHit(r, 233, 389, 7, "menu-exit-pressed");
    checkDrag(r, 233, 192, 233, 140, 233, 192);
    buttons(true, false); frame(r); touchDown(r, 233, 192); check(r.screen().pressedIndex == -1, "kl");
    clearTouch(r); release(r);

    select(r, MenuItem::Records); checkHit(r, 318, 182, 1); checkHit(r, 233, 389, 4);
    select(r, 0); hold(r, false, true, 1500); uint16_t sel = r.screen().selected;
    check(sel >= 10, "r0"); release(r); check(r.screen().selected == sel, "r1");

    tap(r, 233, 35); tap(r, 233, 389); select(r, MenuItem::Partner); select(r, 0); checkEq(r, "list-normal", true);
    checkHit(r, 233, 330, 1, "list-row-pressed"); checkHit(r, 233, 238, 0, "list-active-pressed");
    checkHit(r, 298, 430, -1, "pager-next-pressed", PressedAux::NextPage); checkHit(r, 112, 52, -1, "back-pressed", PressedAux::Back);
    checkDrag(r, 233, 330, 233, 340, 233, 348); checkDrag(r, 112, 52, 112, 85, 112, 85);
    hold(r, false, true, 900); sel = r.screen().selected; check(sel >= 4 && sel < 7, "p0");
    release(r); check(r.screen().selected == sel, "p1");

    tap(r, 233, 35); tap(r, 233, 35); select(r, MenuItem::Pokedex);
    edge(r, false, true); check(viewer_screen::species() == 2, "v0");
    chord(r, true, 60); release(r); check(viewer_screen::species() == 2, "vc");
    hold(r, false, true, 1600); uint16_t sp = viewer_screen::species(); check(sp >= 12, "vr");
    release(r); check(viewer_screen::species() == sp, "vl");
    hold(r, false, true, 400); check(viewer_screen::species() > sp, "vr2"); sp = viewer_screen::species();
    hold(r, true, true, 200); release(r); check(viewer_screen::species() == sp, "va");
  }
}
}
int main() {
  c152::begin("BUTTON MENU TEST");
  for (const unsigned delay : {20u, 80u, 900u}) {
    staggeredConfirmation(true, delay); staggeredConfirmation(false, delay);
  }
  singleAndNonOverlapping(); clockAndSamePage(); touchCancelsPending();
  wakeAndRecords(); retryOnce(); pauseAndResume(true); pauseAndResume(false);
  repeatAndPressedSuite();
  std::printf("%u button menu Runtime/Button_Class checks, 0 failures\n", checks);
}
