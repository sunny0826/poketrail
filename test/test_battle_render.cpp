#include "support/battle_render_harness.hpp"
#include "support/animation_feedback.hpp"
#include "support/battle_cue_checks.hpp"
#include "support/capture_badge.hpp"
#include <algorithm>
namespace {
using namespace battle_render_test;
using namespace battle_cue_test;
void attackAndDodge() {
  MemoryStore store; fixture(store); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r);
  check(r.engine().battleView().expected == Action::Dodge, "seed 0 opens on a dodge cue");
  const uint32_t first = r.engine().battleView().targetMs;
  check(first - origin == kOpeningMs, "first beat follows the full 3-2-1 opening");
  at(r, origin + 100);
  check(countdownDigit(r.engine().battleView(), host_time_ms) == 3 && countdownDigitShown(3) && noCue(),
        "opening shows 3 with no cue yet");
  check(pauseGlyph(), "pause glyph replaces the pause caption under the title");
  snapshot("battle-countdown-3");
  action(r, Action::Dodge, origin + 700); action(r, Action::Attack, origin + 900);
  check(!r.engine().battleView().consumed && r.engine().battleView().playerHp == kPlayerHp &&
        platform.feedbacks == 0, "keys during the countdown are ignored without damage or vibration");
  at(r, origin + 1100); check(countdownDigitShown(1), "countdown reaches 1");
  at(r, first - 900);
  check(noCue(), "ring stays hidden before the 700ms lead");
  check(noteNear(233 + laneOffset(900), false, theme::kBlue), "lane note for the dodge leads the ring");
  at(r, first - 400);
  check(!countdownDigit(r.engine().battleView(), host_time_ms) && cueOn(Action::Dodge, theme::kBlue),
        "dodge cue circles the partner on the left-key side"); hp(3, 5);
  const int early = approachRadius(Action::Dodge, theme::kBlue);
  check(early >= cueRadius(440) - 2 && early <= cueRadius(400), "approach ring closes toward the target ring");
  check(noteNear(233 + laneOffset(400), false, theme::kBlue) && noteNear(233 + laneOffset(1900), false, theme::kBlue),
        "lane shows the current dodge and previews the next dodge");
  snapshot("battle-cue-dodge");
  at(r, first - 200);
  check(approachRadius(Action::Dodge, theme::kBlue) < early, "approach ring keeps shrinking");
  action(r, Action::Dodge, first - 100); const uint32_t dodge = host_time_ms;
  check(platform.feedbacks == 1 && platform.lastFeedback == 20, "normal dodge vibrates once");
  at(r, dodge + 120); hp(3, 5); actor(4, origin, 129, 232, 2);
  check(animation_feedback_test::label("躲开了", theme::kText, 401), "successful dodge is named as a dodge, not a hit");
  snapshot("battle-dodge-trails");
  at(r, dodge + 450); actor(4, origin, 155, 232, 2); actor(25, origin, 311, 226, 2);
  snapshot("battle-dodge-recovered");
  until(r, Action::Attack);
  const uint32_t target = r.engine().battleView().targetMs;
  at(r, target - 750); actor(4, origin, 155, 232, 2); actor(25, origin, 311, 226, 2);
  at(r, target - 400);
  check(cueOn(Action::Attack, theme::kRed), "attack cue circles the enemy on the right-key side");
  check(noteNear(233 + laneOffset(400), true, theme::kRed), "attack note points right");
  snapshot("battle-idle");
  action(r, Action::Attack, target - 100); const uint32_t settled = target - 100;
  check(r.engine().battleView().consumed && r.engine().battleView().lastTiming == Timing::Normal &&
        r.engine().battleView().enemyHp == 4, "normal input preserves one damage");
  hp(3, 4);
  at(r, settled + 100); actor(4, origin, 189, 232, 2); actor(25, origin, 311, 226, 2, true, theme::kText);
  snapshot("battle-attack-impact");
  at(r, settled + 300);
  check(r.engine().battleView().consumed || r.engine().battleView().cueIndex > 0, "attack settled before the deadline");
  actor(4, origin, 166, 232, 2);
  snapshot("battle-attack-recovery");
}
void perfectAndFinal() {
  MemoryStore store; fixture(store, 4, 41, Route::Forest); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r);
  // seed 0: D D A A D A* ... ; perfect dodges fill energy without hurting the enemy.
  for (unsigned i = 0; i < 2; ++i) {
    action(r, Action::Dodge, r.engine().battleView().targetMs); at(r, host_time_ms + 450);
  }
  check(r.engine().battleView().energy == 2, "perfect dodges build energy");
  check(platform.feedbacks == 2 && platform.lastFeedback == 40, "perfect dodges vibrate longer");
  const uint32_t target = r.engine().battleView().targetMs;
  at(r, target - 20); check(cueOn(Action::Attack, theme::kGold), "target ring turns gold inside the perfect window");
  action(r, Action::Attack, target); hp(3, 3);
  check(r.engine().battleView().energy == kSpecialEnergy && r.engine().battleView().frenzy == false, "third perfect fills energy");
  at(r, target + 125); hp(3, 3); actor(4, origin, 188, 232, 2);
  for (unsigned i = 0; i < kSpecialEnergy; ++i) check(M5.Display.readPixel(209 + i * 24, 326) != theme::kDisabled, "full energy pips light up");
  snapshot("battle-perfect-impact");
  at(r, target + 450); actor(4, origin, 155, 232, 2); actor(41, origin, 311, 226, 2);
  check(!r.engine().battleView().frenzy, "3 of 5 HP is not yet below half");
  // Next cue (attack): a normal hit leaves 2 HP and the enemy turns frenzied.
  action(r, Action::Attack, r.engine().battleView().targetMs - 100); at(r, host_time_ms + 450);
  hp(3, 2);
  until(r, Action::Attack);
  check(r.engine().battleView().frenzy && r.engine().battleView().energy == kSpecialEnergy, "enemy below half HP turns frenzied");
  check(r.engine().battleView().feint, "seed 0 cue 5 is a feint");
  // Frenzy blinks white on odd 90ms slots; pick the even (coloured) slot at or after t.
  const auto blinkOff = [](uint32_t t) { return t + (t / 90) % 2 * (90 - t % 90); };
  // blinkOff may move up to 89ms later; starting 440ms out keeps the sample before the 350ms flip.
  at(r, blinkOff(r.engine().battleView().targetMs - 440));
  check(cueOn(Action::Dodge, theme::kBlue), "feint decoy circles the partner early");
  check(r.engine().battleView().expected == Action::Attack, "decoy never changes the judged action");
  snapshot("battle-feint-decoy");
  at(r, blinkOff(r.engine().battleView().targetMs - 300));
  check(cueOn(Action::Attack, theme::kRed), "feint jumps to the enemy 350ms before the beat");
  // Use a white slot 80–170ms before the beat, at least 40ms after the last render.
  uint32_t blink = r.engine().battleView().targetMs - 170;
  if (!(blink / 90 % 2)) blink += 90 - blink % 90;
  check(blink - host_time_ms >= 40, "frenzy sample allows a fresh Runtime frame");
  at(r, blink);
  check((host_time_ms / 90) % 2 && cueOn(Action::Attack, theme::kText), "frenzy cue blinks white while it closes");
  snapshot("battle-frenzy-special-ready");
  const uint32_t final = r.engine().battleView().targetMs; const unsigned commits = store.commits;
  const unsigned buzzes = platform.feedbacks;
  buttons(true, true); at(r, final);
  check(r.engine().battleView().special && r.engine().battleView().enemyHp == 0, "real two-key chord lands the special");
  check(platform.feedbacks == buzzes + 1 && platform.lastFeedback == 40, "special vibrates like a perfect hit");
  check(r.screen().page == Page::Battle && r.engine().view().phase == Phase::ThrowReady && store.commits == commits + 1,
        "final hit commits victory immediately while battle stays visible");
  Save saved; check(store.load(saved).status == LoadStatus::Loaded && saved.phase == Phase::ThrowReady, "victory already survives codec reload during final animation");
  at(r, final + 125); hp(3, 0);
  check(animation_feedback_test::label("必杀命中", theme::kGold, 401), "special hit has explicit feedback");
  snapshot("battle-final-special");
  at(r, final + 449); check(r.screen().page == Page::Battle, "final action remains until 449ms");
  at(r, final + 450); check(r.screen().page == Page::Throw && !r.engine().throwView().active, "450ms enters throw without reusing winning key");
  at(r, final + 600); check(!r.engine().throwView().active, "held winning keys cannot start throw");
  buttons(false, false); frame(r);
  buttons(false, true); frame(r);
  check(r.engine().throwView().active && !r.engine().view().encounter.usedBalls, "fresh key prepares first ball without spending it");
  frame(r, 40);
  check(capture_badge_test::iconAt(ballSlotX(0), kBallSlotY) && capture_badge_test::iconAt(ballSlotX(1), kBallSlotY),
        "throw title shows both unused attempts as balls");
}
void failureAndPractice() {
  for (unsigned practice = 0; practice < 2; ++practice) {
    MemoryStore store; fixture(store, 7, 54, Route::Coast); HostPlatform platform; Runtime r(store, platform); start(r);
    enter(r, practice); const unsigned commits = store.commits;
    unsigned missedDodges = 0;
    for (unsigned cue = 0; cue < kMaxCues; ++cue) {
      const auto b = practice ? r.practiceView() : r.engine().battleView();
      if (!b.active) break;
      const unsigned buzzes = platform.feedbacks;
      at(r, b.targetMs + 181);
      check(platform.feedbacks == buzzes + 1 && platform.lastFeedback == 10, "a missed beat vibrates briefly");
      if (b.expected == Action::Dodge && ++missedDodges == 1) {
        at(r, host_time_ms + 120); hp(2, 5);
        check(animation_feedback_test::label("受伤了", theme::kMuted, 401), "failed dodge identifies the damage outcome");
        snapshot(practice ? "practice-miss" : "battle-miss");
      }
    }
    const auto b = practice ? r.practiceView() : r.engine().battleView();
    check(!b.active && b.result == Result::Lost && b.playerHp == 0 && missedDodges == kPlayerHp, "three failed dodges still end battle");
    check(r.screen().page == (practice ? Page::Practice : Page::Battle), "loss has a final presentation window");
    at(r, b.settledMs + 449); check(r.screen().page == (practice ? Page::Practice : Page::Battle), "loss remains before 450ms");
    at(r, b.settledMs + 450); check(r.screen().page == (practice ? Page::PracticeResult : Page::Result), "loss routes at 450ms");
    if (practice) check(store.commits == commits && r.engine().view().phase == Phase::EncounterReady, "practice grants nothing and retains encounter");
  }
  MemoryStore store; fixture(store); HostPlatform platform; Runtime r(store, platform); start(r); enter(r);
  for (;;) {
    const auto b = r.engine().battleView();
    const bool finalHit = b.expected == Action::Attack && b.enemyHp <= 2;
    if (finalHit) store.fail = true;
    action(r, b.expected, b.targetMs);
    if (finalHit) break;
    at(r, host_time_ms + 450);
  }
  check(r.screen().page == Page::Error && r.engine().savePending() && r.engine().view().phase == Phase::Battle,
        "failed final commit takes priority over animation and never publishes victory");
  snapshot("battle-save-failure");
  store.fail = false; frame(r); select(r, 0);
  check(r.screen().page == Page::Throw && r.engine().view().phase == Phase::ThrowReady, "retry publishes the original victory once");
}
void newGenerationActors() {
  const uint16_t partners[] = {252, 386, 470, 597, 600, 606, 615, 642, 644, 648, 649};
  for (uint16_t partner : partners) {
    MemoryStore store; fixture(store, partner, 597, Route::Cave);
    HostPlatform platform; Runtime runtime(store, platform); start(runtime);
    const uint32_t origin = enter(runtime);
    at(runtime, runtime.engine().battleView().targetMs - 750);
    const auto& source = data::kSprites[partner - 1];
    const auto& enemy = data::kSprites[596];
    actor(partner, origin, 155, 232, source.width <= 68 && source.height <= 65 ? 2 : 1);
    actor(597, origin, 311, 226, enemy.width <= 68 && enemy.height <= 65 ? 2 : 1);
    char name[48]; std::snprintf(name, sizeof(name), "battle-gen5-partner-%u", partner);
    snapshot(name);
  }
}
void timingAndPause() {
  Battle b; b.begin(UINT32_MAX - kOpeningMs + 1);
  check(!battleEffectActive(b.view(), UINT32_MAX - kOpeningMs + 1), "begin never plays an old effect");
  check(b.press(Action::Attack, 0), "settlement at millis zero is accepted");
  check(battleEffectActive(b.view(), 0) && battleEffectActive(b.view(), 449), "zero settlement supports complete effect");
  b.tick(181); check(battleEffectAction(b.view()) == Action::Dodge && b.view().expected == Action::Dodge && battleEffectActive(b.view(), 300), "effect keeps the settled cue, not the next cue or the key pressed");
  check(!battleEffectActive(b.view(), 450), "effect ends exactly at 450ms");
  b.requestPause(); b.tick(b.view().targetMs + 181); const uint32_t pause = b.view().settledMs;
  check(!battleEffectActive(b.view(), pause), "paused battle suppresses effects");
  b.resume(pause + 1); check(!battleEffectActive(b.view(), pause + 1), "resume does not replay prior cue");
  b.begin(UINT32_MAX - 200 - kOpeningMs); b.press(Action::Attack, UINT32_MAX - 200); b.tick(50);
  check(battleEffectActive(b.view(), 100) && battleEffectAction(b.view()) == Action::Dodge, "effect timing survives millis rollover");
  MemoryStore store; fixture(store, 17, 25, Route::Coast); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r); at(r, r.engine().battleView().targetMs - 750);
  actor(17, origin, 155, 232, 1); actor(25, origin, 311, 226, 2); snapshot("battle-tall-partner");
  tap(r, 233, 30); at(r, r.engine().battleView().targetMs + 181);
  check(r.screen().page == Page::Pause, "real top touch pauses on cue boundary"); screenshot(r, "battle-paused");
  select(r, 0); check(r.screen().page == Page::Battle && !battleEffectActive(r.engine().battleView(), host_time_ms), "real resume begins a clean beat");
  frame(r, 40);
  check(countdownDigit(r.engine().battleView(), host_time_ms) == 3 && countdownDigitShown(3) && noCue(),
        "resume counts 3-2-1 again before the next cue");
  snapshot("battle-resumed");
}
}
int main() {
  c152::begin("BATTLE TEST");
  attackAndDodge(); perfectAndFinal(); failureAndPractice(); newGenerationActors(); timingAndPause();
  std::printf("%u battle Runtime/Button_Class/M5GFX checks, 0 failures\n", checks);
}
