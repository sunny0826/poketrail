#include "support/progression_ui.hpp"
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
  openMenu(r); select(r, 5); select(r, 0); select(r, 1);
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
  if (practice) select(r, 6); else { select(r, 0); select(r, 0); }
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
}
int main() {
  c152::begin("BUTTON MENU TEST");
  for (const unsigned delay : {20u, 80u, 900u}) {
    staggeredConfirmation(true, delay); staggeredConfirmation(false, delay);
  }
  singleAndNonOverlapping(); clockAndSamePage(); touchCancelsPending();
  wakeAndRecords(); retryOnce(); pauseAndResume(true); pauseAndResume(false);
  std::printf("%u button menu Runtime/Button_Class checks, 0 failures\n", checks);
}
