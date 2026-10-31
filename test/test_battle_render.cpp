#include "support/progression_ui.hpp"
#include "battle_presentation.hpp"
#include "support/animation_feedback.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
using namespace progression_ui;
namespace {
void fixture(MemoryStore& store, uint16_t partner = 4, uint16_t enemy = 25, Route route = Route::Coast) {
  Save save; save.bootGeneration = 1; save.starter = 1; save.partner = partner;
  save.rosterRevision = 1; setOwned(save, 1); setRegistered(save, 1);
  setOwned(save, partner); setRegistered(save, partner);
  save.encounterTarget = 300;
  save.totalSteps = save.undatedSteps = 5000; save.explorationSpent = 5000; save.nextEncounterId = 2;
  save.phase = Phase::EncounterReady; save.route = save.encounter.route = route;
  save.encounter.id = 1; save.encounter.species = enemy; save.encounter.partner = partner;
  check(validate(save) && store.commit(save) == CommitStatus::Committed, "battle fixture validates through production codec");
}
uint32_t enter(Runtime& r, bool practice = false) {
  openMenu(r);
  if (!practice) select(r, 0);
  else for (unsigned i = 0; i < 5; ++i) edge(r, false, true);
  buttons(true, true); frame(r);
  const uint32_t origin = host_time_ms;
  check(r.screen().page == (practice ? Page::Practice : Page::Battle), "real chord enters battle surface");
  buttons(false, false); frame(r); frame(r);
  return origin;
}
void at(Runtime& r, uint32_t time) { frame(r, time - host_time_ms); }
void action(Runtime& r, Action action, uint32_t time) {
  buttons(action == Action::Dodge, action == Action::Attack); at(r, time);
  buttons(false, false); frame(r, 0);
}
unsigned frameIndex(const Sprite& sprite, uint32_t elapsed) {
  uint32_t t = elapsed % sprite.duration_ms;
  for (unsigned i = 0; i < sprite.frame_count; ++i) {
    if (t < sprite.frames[i].duration_ms) return i;
    t -= sprite.frames[i].duration_ms;
  }
  check(false, "source frame timing is complete"); return 0;
}
void actor(uint16_t id, uint32_t origin, int cx, int cy, unsigned scale, bool upperOnly = false, uint16_t silhouette = 0) {
  const auto& sprite = data::kSprites[id - 1];
  const unsigned width = sprite.width * scale, height = sprite.height * scale;
  std::vector<uint16_t> pixels(width * height);
  check(decodeFrame(sprite, frameIndex(sprite, host_time_ms - origin), scale, pixels.data(), pixels.size()), "original animation frame decodes");
  unsigned ink = 0, mismatch = 0;
  for (unsigned y = 0; y < (upperOnly ? height / 4 : height); ++y) for (unsigned x = 0; x < width; ++x) {
    const uint16_t pixel = pixels[y * width + x];
    if (pixel == kTransparentKey) continue;
    ++ink;
    if (M5.Display.readPixel(cx - int(width) / 2 + x, cy - int(height) / 2 + y) != (silhouette ? silhouette : pixel)) ++mismatch;
  }
  if (mismatch) std::fprintf(stderr, "actor #%u at %u origin %u center %d,%d mismatches=%u/%u\n", id, host_time_ms, origin, cx, cy, mismatch, ink);
  check(ink > 20 && mismatch == 0, "visible actor preserves original frame duration, integer pixels and placement");
}
void hp(unsigned player, unsigned enemy) {
  for (unsigned i = 0; i < kPlayerHp; ++i)
    check(M5.Display.readPixel(92 + i * 18, 110) == (i < player ? theme::kMint : theme::kDisabled), "player HP immediately matches battle result");
  for (unsigned i = 0; i < kEnemyHp; ++i)
    check(M5.Display.readPixel(283 + i * 11, 110) == (i < enemy ? theme::kGold : theme::kDisabled), "enemy HP immediately matches battle result");
}
// Plays cues until the next one requires `want`, using perfect dodges and normal attacks.
void until(Runtime& r, Action want) {
  while (r.engine().battleView().expected != want) {
    const auto b = r.engine().battleView();
    action(r, b.expected, b.targetMs - (b.expected == Action::Attack ? 100 : 0));
    at(r, host_time_ms + 450);
  }
}
bool ringPixel(uint16_t color) {
  // At 400ms before the beat over half the arc remains: 3 and 6 o'clock are lit, 9 o'clock is not.
  return M5.Display.readPixel(451, 233) == color && M5.Display.readPixel(233, 451) == color &&
         M5.Display.readPixel(15, 233) != color;
}
void snapshot(const char* name) {
  const std::string path = verificationOutput((std::string(name) + ".ppm").c_str());
  FILE* file = std::fopen(path.c_str(), "wb"); check(file, "open battle screenshot");
  std::fprintf(file, "P6\n468 468\n255\n"); bool safe = true;
  for (int y = 0; y < 468; ++y) for (int x = 0; x < 468; ++x) {
    const uint16_t pixel = M5.Display.readPixel(x, y);
    if (pixel && (x - 233) * (x - 233) + (y - 233) * (y - 233) > 228 * 228) safe = false;
    const uint8_t rgb[] = {uint8_t((pixel >> 11) * 255 / 31), uint8_t(((pixel >> 5) & 63) * 255 / 63), uint8_t((pixel & 31) * 255 / 31)};
    std::fwrite(rgb, 1, 3, file);
  }
  std::fclose(file); check(safe, "battle frame fits circular screen");
}
void attackAndDodge() {
  MemoryStore store; fixture(store); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r);
  check(r.engine().battleView().expected == Action::Dodge, "seed 0 opens on a dodge cue");
  at(r, r.engine().battleView().targetMs - 900);
  check(M5.Display.readPixel(451, 233) != theme::kBlue && M5.Display.readPixel(451, 233) != theme::kRed, "ring stays hidden before the 700ms lead");
  at(r, r.engine().battleView().targetMs - 400);
  check(ringPixel(theme::kBlue), "blue ring announces a dodge"); hp(3, 5);
  snapshot("battle-cue-dodge");
  action(r, Action::Dodge, r.engine().battleView().targetMs - 100); const uint32_t dodge = host_time_ms;
  at(r, dodge + 120); hp(3, 5); actor(4, origin, 129, 232, 2);
  check(animation_feedback_test::label("躲开了", theme::kText, 401), "successful dodge is named as a dodge, not a hit");
  snapshot("battle-dodge-trails");
  at(r, dodge + 450); actor(4, origin, 155, 232, 2); actor(25, origin, 311, 226, 2);
  snapshot("battle-dodge-recovered");
  until(r, Action::Attack);
  const uint32_t target = r.engine().battleView().targetMs;
  at(r, target - 400); actor(4, origin, 155, 232, 2); actor(25, origin, 311, 226, 2);
  check(ringPixel(theme::kRed), "red ring announces an attack");
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
  const uint32_t target = r.engine().battleView().targetMs;
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
  at(r, blinkOff(r.engine().battleView().targetMs - 400));
  check(M5.Display.readPixel(451, 233) == theme::kBlue, "feint decoy shows a dodge ring early");
  check(r.engine().battleView().expected == Action::Attack, "decoy never changes the judged action");
  snapshot("battle-feint-decoy");
  at(r, blinkOff(r.engine().battleView().targetMs - 300));
  check(M5.Display.readPixel(451, 233) == theme::kRed, "feint flips to the real attack 350ms before the beat");
  // Use a white slot 80–170ms before the beat, at least 40ms after the last render.
  // This keeps 1 o'clock inside the closing arc and 3 o'clock outside it.
  uint32_t blink = r.engine().battleView().targetMs - 170;
  if (!(blink / 90 % 2)) blink += 90 - blink % 90;
  check(blink - host_time_ms >= 40, "frenzy sample allows a fresh Runtime frame");
  at(r, blink);
  check((host_time_ms / 90) % 2 && M5.Display.readPixel(342, 44) == theme::kText &&
        M5.Display.readPixel(451, 233) != theme::kText, "frenzy ring blinks white while it closes");
  snapshot("battle-frenzy-special-ready");
  const uint32_t final = r.engine().battleView().targetMs; const unsigned commits = store.commits;
  buttons(true, true); at(r, final);
  check(r.engine().battleView().special && r.engine().battleView().enemyHp == 0, "real two-key chord lands the special");
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
}
void failureAndPractice() {
  for (unsigned practice = 0; practice < 2; ++practice) {
    MemoryStore store; fixture(store, 7, 54, Route::Coast); HostPlatform platform; Runtime r(store, platform); start(r);
    enter(r, practice); const unsigned commits = store.commits;
    unsigned missedDodges = 0;
    for (unsigned cue = 0; cue < kMaxCues; ++cue) {
      const auto b = practice ? r.practiceView() : r.engine().battleView();
      if (!b.active) break;
      at(r, b.targetMs + 181);
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
    at(runtime, runtime.engine().battleView().targetMs - 400);
    const auto& source = data::kSprites[partner - 1];
    const auto& enemy = data::kSprites[596];
    actor(partner, origin, 155, 232, source.width <= 68 && source.height <= 65 ? 2 : 1);
    actor(597, origin, 311, 226, enemy.width <= 68 && enemy.height <= 65 ? 2 : 1);
    char name[48]; std::snprintf(name, sizeof(name), "battle-gen5-partner-%u", partner);
    snapshot(name);
  }
}
void timingAndPause() {
  Battle b; b.begin(UINT32_MAX - 1499);
  check(!battleEffectActive(b.view(), UINT32_MAX - 1499), "begin never plays an old effect");
  check(b.press(Action::Attack, 0), "settlement at millis zero is accepted");
  check(battleEffectActive(b.view(), 0) && battleEffectActive(b.view(), 449), "zero settlement supports complete effect");
  b.tick(181); check(battleEffectAction(b.view()) == Action::Dodge && b.view().expected == Action::Dodge && battleEffectActive(b.view(), 300), "effect keeps the settled cue, not the next cue or the key pressed");
  check(!battleEffectActive(b.view(), 450), "effect ends exactly at 450ms");
  b.requestPause(); b.tick(b.view().targetMs + 181); const uint32_t pause = b.view().settledMs;
  check(!battleEffectActive(b.view(), pause), "paused battle suppresses effects");
  b.resume(pause + 1); check(!battleEffectActive(b.view(), pause + 1), "resume does not replay prior cue");
  b.begin(UINT32_MAX - 1700); b.press(Action::Attack, UINT32_MAX - 200); b.tick(50);
  check(battleEffectActive(b.view(), 100) && battleEffectAction(b.view()) == Action::Dodge, "effect timing survives millis rollover");
  MemoryStore store; fixture(store, 17, 25, Route::Coast); HostPlatform platform; Runtime r(store, platform); start(r);
  const uint32_t origin = enter(r); at(r, r.engine().battleView().targetMs - 400);
  actor(17, origin, 155, 232, 1); actor(25, origin, 311, 226, 2); snapshot("battle-tall-partner");
  tap(r, 233, 30); at(r, r.engine().battleView().targetMs + 181);
  check(r.screen().page == Page::Pause, "real top touch pauses on cue boundary"); screenshot(r, "battle-paused");
  select(r, 0); check(r.screen().page == Page::Battle && !battleEffectActive(r.engine().battleView(), host_time_ms), "real resume begins a clean beat");
  snapshot("battle-resumed");
}
}
int main() {
  c152::begin("BATTLE TEST");
  attackAndDodge(); perfectAndFinal(); failureAndPractice(); newGenerationActors(); timingAndPause();
  std::printf("%u battle Runtime/Button_Class/M5GFX checks, 0 failures\n", checks);
}
