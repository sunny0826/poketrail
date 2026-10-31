#include "driver.hpp"
#include "viewer_screen.hpp"
#include "capture_badge_checks.hpp"

namespace simulator {
namespace e2e {
namespace {

// Frozen from observed runs; acceptance never searches seeds or recomputes odds.
constexpr uint16_t kThrowSpecies = 72;
constexpr uint32_t kThrowSeed = 0x2468ace0u;

// 投球后等待结算落定：提交完成进 Result，保存故障进 Error，
// 未结束时回到 Throw。
void waitSettled(Driver& d, const char* what) {
  d.until(
      [&] {
        const Page p = d.screen().page;
        return p == Page::Result || p == Page::Error || p == Page::Throw;
      },
      20000, what);
}

unsigned captureSpeciesTotal(const Save& s) {
  unsigned total = 0;
  for (uint16_t id = 1; id <= kSpeciesCount; ++id)
    if (s.captureCounts[id - 1]) ++total;
  return total;
}

uint16_t capturedSpecies(const Save& s) {
  for (uint16_t id = 1; id <= kSpeciesCount; ++id)
    if (s.captureCounts[id - 1]) return id;
  return 0;
}

// 新档引导：跳过日期 → 选择伙伴 1 → Home（路径与 smoke 一致）
void bootNewSave(Driver& d) {
  d.frames();
  d.check(d.screen().page == Page::DateWelcome, "new save opens date welcome");
  d.shot("date-welcome");
  d.press(SDL_SCANCODE_RIGHT);
  d.press(SDL_SCANCODE_RETURN);
  d.until([&] { return d.screen().page == Page::Starter; }, 4000, "starter page after skipping date");
  d.shot("starter");
  d.select(0);
  d.until([&] { return d.screen().page == Page::Home; }, 4000, "home after choosing starter");
  d.check(d.save().starter == 1, "starter 1 selected");
}

// EncounterReady 遭遇从菜单推进到战斗并真实获胜
void enterAndWinBattle(Driver& d, bool fixed = false) {
  d.homeMenu();
  d.select(0);
  d.until([&] { return d.screen().page == Page::Encounter; }, 4000, "menu opens the encounter");
  d.select(0);
  d.until([&] { return d.screen().page == Page::Battle; }, 4000, "battle starts");
  d.shot("battle-start");
  if (fixed) d.replayBattle(); else d.winBattle();
  d.until([&] { return d.save().phase == Phase::ThrowReady; }, 30000, "throw ready after winning");
  d.shot("throw-ready");
}

// Result 页确认（selected 0），遭遇收尾回 Home
void acknowledge(Driver& d) {
  const auto completed = d.save().encounter.id;
  d.press(SDL_SCANCODE_RETURN);
  d.until([&] { return d.screen().page == Page::Home; }, 4000, "result acknowledged to home");
  d.check(d.save().encounter.id != completed && d.save().result == Result::None,
          "acknowledge closes old encounter; saved credit may start the next");
}

// 捕获记录的数据级断言（供 run 与 reload 复用）
void checkCaptureRecord(Driver& d, uint16_t species) {
  const Save& s = d.save();
  d.check(s.captureCounts[species - 1] == 1, "exactly one capture of the species");
  d.check(owns(s, species), "species marked owned");
  d.check(s.recentSize >= 1 && s.recent[s.recentSize - 1].species == species,
          "capture appended to recent records");
  d.check(!s.firstCapture[species - 1].at.timeKnown, "first capture keeps unknown date");
  d.check(s.rosterRevision >= 1, "roster revision advanced");
}

}  // namespace

// P0-01：新档跳过日期 → 选伙伴 → 走 500 步 → 菜单/图鉴 → 关闭；
// reload 断言步数、伙伴与在途遭遇恢复。
void scenarioJourney(Driver& d, const std::string& phase) {
  if (phase == "run") {
    bootNewSave(d);
    d.shot("home-starter");
    d.homeMenu();
    d.check(d.screen().page == Page::Menu, "keyboard opens menu");
    d.select(3);
    d.until([&] { return d.screen().page == Page::Viewer; }, 4000, "pokedex viewer opens");
    d.press(SDL_SCANCODE_RIGHT);
    d.frames(8);
    d.shot("viewer-browsing");
    d.back();
    d.until([&] { return d.screen().page == Page::Home; }, 4000, "back home from viewer");
    d.walk(500);
    d.check(d.save().totalSteps >= 500, "synthetic walk crossed 500 steps");
    d.check(d.save().phase == Phase::EncounterReady, "walking produced an encounter");
    d.shot("encounter-ready");
    d.event("journey-run-done", "steps=" + std::to_string(d.save().totalSteps) +
                                    " encounter=" + std::to_string(d.save().encounter.id));
    d.close();
  } else if (phase == "reload") {
    d.frames();
    d.check(d.screen().page == Page::Home, "reload lands on home");
    d.check(d.save().starter == 1, "starter survives restart");
    d.check(d.save().totalSteps >= 500, "steps survive restart");
    d.check(d.save().phase == Phase::EncounterReady, "encounter survives restart");
    d.shot("journey-reload");
    d.close();
  }
}

// P0-02：新档走路遭遇 → 真实对战获胜 → 完美投球捕获 → 确认；
// 检查图鉴/伙伴/记录入口与未知日期，reload 断言无重复奖励。
void scenarioCapture(Driver& d, const std::string& phase) {
  if (phase == "run" || phase == "fixed") {
    bootNewSave(d);
    checkCaptureBadge(d, 0, "gifted-starter");
    d.walk(500);
    d.check(d.save().phase == Phase::EncounterReady, "encounter ready after walk");
    const uint16_t species = d.save().encounter.species;
    d.check(species == 590 && d.save().encounter.seed == 1745833800u, "frozen new-save encounter package");
    d.event("capture-encounter", "species=" + std::to_string(species) +
                                     " seed=" + std::to_string(d.save().encounter.seed));
    enterAndWinBattle(d, phase == "fixed");
    d.check(!capture_badge_test::iconAt(312, 109), "throw page removes the static ball badge");
    d.throwBall(true);
    d.finishCapture();
    waitSettled(d, "commit settles after the throw");
    d.check(d.screen().page == Page::Result, "result page shown only after commit");
    d.check(d.save().result == Result::Captured, "capture result published");
    d.check(d.save().encounter.attempts[0].roll == 33 && d.save().encounter.attempts[0].probability == 90,
            "frozen perfect capture roll and probability");
    checkCaptureRecord(d, species);
    d.shot("result-captured");
    acknowledge(d);
    checkCaptureBadge(d, 1, "first-capture");
    d.homeMenu();
    d.select(3);
    d.until([&] { return d.screen().page == Page::Viewer; }, 4000, "viewer opens after capture");
    d.frames();
    for (unsigned i = 0; pokedex::viewer_screen::species() != species && i < kSpeciesCount; ++i)
      d.press(SDL_SCANCODE_LEFT);
    d.check(pokedex::viewer_screen::species() == species, "viewer displays captured species");
    d.shot("viewer-after-capture");
    d.back();
    d.until([&] { return d.screen().page == Page::Home; }, 4000, "back home from viewer");
    d.homeMenu();
    d.select(2);
    d.until([&] { return d.screen().page == Page::Partners; }, 4000, "partners list opens");
    d.select(1);
    d.until([&] { return d.screen().page == Page::PartnerDetail; }, 4000, "partner detail opens");
    d.check(d.screen().species == species, "partner detail shows captured species");
    d.shot("partner-detail");
    d.back();
    d.until([&] { return d.screen().page == Page::Partners; }, 4000, "back to partners");
    d.back();
    d.until([&] { return d.screen().page == Page::Menu; }, 4000, "back to menu");
    d.select(4);
    d.until([&] { return d.screen().page == Page::Resources; }, 4000, "resources opens");
    d.select(0);
    d.until([&] { return d.screen().page == Page::Records; }, 4000, "records opens");
    d.select(0);
    d.until([&] { return d.screen().page == Page::FirstCaptures; }, 4000, "first captures opens");
    d.frames();
    for (unsigned i = 0; d.screen().selected != species - 1 && i <= kSpeciesCount; ++i)
      d.press(SDL_SCANCODE_LEFT);
    d.check(d.screen().selected == species - 1 && d.save().firstCapture[d.screen().selected].species == species,
            "first capture card displays actual capture");
    d.shot("first-captures");
    d.back();
    d.until([&] { return d.screen().page == Page::Records; }, 4000, "back to records");
    d.back();
    d.until([&] { return d.screen().page == Page::Resources; }, 4000, "back to resources");
    d.back();
    d.until([&] { return d.screen().page == Page::Menu; }, 4000, "back to menu");
    d.back();
    d.until([&] { return d.screen().page == Page::Home; }, 4000, "back home from menu");
    d.event("capture-run-done", "species=" + std::to_string(species));
    d.close();
  } else if (phase == "reload") {
    d.frames();
    d.check(d.screen().page == Page::Home, "reload lands on home");
    d.check(d.save().starter == 1, "starter survives restart");
    d.check(d.save().totalSteps >= 500, "steps survive restart");
    d.check(captureSpeciesTotal(d.save()) == 1, "exactly one capture after restart");
    checkCaptureRecord(d, capturedSpecies(d.save()));
    d.check(d.save().phase == Phase::EncounterReady && d.save().encounter.id == 2, "credit-funded next encounter survives restart");
    checkCaptureBadge(d, 1, "capture-reloaded");
    d.shot("capture-reload");
    d.close();
  }
}

// P0-03：throw fixture（species 72、rule3）两投均 Miss 恰好 Escaped，
// 库存不变，确认后走路开启新 ID；aim 变体证明空等与轻触都不消耗机会，
// aim-reload 断言 ThrowReady 与已消耗的一球恢复且窗口不复活。
void scenarioThrows(Driver& d, const std::string& phase) {
  if (phase == "run") {
    d.frames();
    d.check(d.screen().page == Page::Throw, "throw fixture opens throw page");
    d.check(d.save().phase == Phase::ThrowReady, "fixture starts throw ready");
    d.check(d.save().encounter.species == kThrowSpecies, "fixture species 72");
    d.check(d.save().encounter.usedBalls == 0, "fixture has no attempts");
    d.check(d.save().encounter.ruleVersion == 3, "fixture uses rule 3");
    d.check(d.save().encounter.seed == kThrowSeed, "fixture seed matches frozen value");
    const uint64_t ballsSpent0 = d.save().ballsSpent;
    const uint64_t encounterId = d.save().encounter.id;
    d.throwBall(false);
    d.finishCapture();
    waitSettled(d, "first throw settles");
    d.check(d.screen().page == Page::Throw, "first miss returns to throw page");
    d.check(d.save().encounter.usedBalls == 1, "first attempt recorded");
    d.check(d.save().encounter.attempts[0].used && d.save().encounter.attempts[0].timing == Timing::Miss,
            "first attempt missed");
    d.shot("throw-first-miss");
    d.throwBall(false);
    d.finishCapture();
    waitSettled(d, "second throw settles");
    d.check(d.screen().page == Page::Result, "second miss resolves the encounter");
    d.check(d.save().result == Result::Escaped, "encounter escaped");
    d.check(d.save().phase == Phase::Resolved, "encounter resolved");
    d.check(d.save().encounter.usedBalls == 2, "exactly two attempts");
    d.check(d.save().encounter.attempts[0].roll == 16 && d.save().encounter.attempts[1].roll == 5 &&
            d.save().encounter.attempts[0].probability == 0 && d.save().encounter.attempts[1].probability == 0,
            "frozen two-miss package");
    d.check(!owns(d.save(), kThrowSpecies) && !d.save().captureCounts[kThrowSpecies - 1],
            "no capture recorded");
    d.check(d.save().ballsSpent == ballsSpent0, "rule 3 leaves ball stock untouched");
    d.shot("throw-escaped");
    acknowledge(d);
    d.check(captureSpeciesTotal(d.save()) == 0, "records unchanged after escape");
    d.walk(500);
    d.check(d.save().phase == Phase::EncounterReady, "next encounter starts after walking");
    d.check(d.save().encounter.id != encounterId, "next encounter uses a new id");
    d.check(d.save().encounter.usedBalls == 0, "next encounter is fresh");
    d.shot("next-encounter");
    d.event("throws-run-done", "old=" + std::to_string(encounterId) +
                                   " new=" + std::to_string(d.save().encounter.id));
    d.close();
  } else if (phase == "reload") {
    d.frames();
    d.check(d.save().phase == Phase::EncounterReady, "new encounter survives restart");
    d.check(d.save().encounter.usedBalls == 0, "new encounter has no attempts");
    d.shot("throws-reload");
    d.close();
  } else if (phase == "aim") {
    aimBeforeThrow(d);
    d.event("throws-aim-done", "id=" + std::to_string(d.save().encounter.id));
    d.close();
  } else if (phase == "aim-reload") {
    d.frames();
    d.until([&] { return d.screen().page == Page::Throw; }, 4000, "the throw page reopens");
    d.check(d.save().phase == Phase::ThrowReady, "the pending throw survives restart");
    d.check(d.save().encounter.usedBalls == 1, "the spent attempt is still recorded");
    d.check(!d.runtime().engine().throwView().active,
            "restart restores the aiming state, not the RAM throw window");
    d.advance(3000);
    d.check(d.save().encounter.usedBalls == 1,
            "waiting after restart spends no further attempt");
    d.shot("aim-reload");
    d.close();
  }
}

// Each fault starts from its own fixture. Hold the retry key across successful
// publication to prove it cannot also acknowledge Result without a release.
void scenarioRetry(Driver& d, const std::string& phase) {
  if (phase == "before" || phase == "after" || phase == "readback") {
    d.frames();
    d.check(d.screen().page == Page::Throw, "throw fixture opens throw page");
    d.check(d.save().encounter.species == kThrowSpecies, "fixture species 72");
    d.event("retry-seed", "seed=" + std::to_string(d.save().encounter.seed) +
                              " rule=" + std::to_string(d.save().encounter.ruleVersion));
    // pressThrow 在松开瞬间同步提交，故障必须先武装才命中该次写入；
    // 提交失败时产品不启动捕获动画，Error 直接替换结果页。
    d.fault(phase == "before" ? Fault::BeforeWrite : phase == "after" ? Fault::AfterWrite : Fault::ReadBack);
    d.throwBall(true);
    d.check(!d.screen().capture.active, "failed commit starts no reward animation");
    d.check(d.screen().page == Page::Error, "error page replaces result before commit");
    d.check(!owns(d.save(), kThrowSpecies), "no reward before commit");
    d.check(d.save().captureCounts[kThrowSpecies - 1] == 0, "no capture count before commit");
    d.check(d.save().result == Result::Won && d.save().encounter.usedBalls == 0, "failed save preserves pre-throw published state");
    d.check(d.faultHits() == 1, "exactly one fault hit");
    d.shot("retry-error");
    d.fault(Fault::None);
    d.key(SDL_SCANCODE_RETURN, true);
    d.frames(20);
    d.check(d.screen().page == Page::Result, "held retry confirms once and cannot acknowledge the result");
    d.key(SDL_SCANCODE_RETURN, false);
    d.frames(4);
    d.until([&] { return d.screen().page == Page::Result; }, 20000, "retry commits the same candidate");
    d.check(d.save().result == Result::Captured, "retry preserves the capture outcome");
    d.check(d.save().captureCounts[kThrowSpecies - 1] == 1, "exactly one reward after retry");
    const ThrowAttempt& a = d.save().encounter.attempts[0];
    d.check(a.used && a.timing == Timing::Perfect && a.captured && a.roll == 16, "frozen attempt preserved verbatim");
    d.event("retry-attempt", "roll=" + std::to_string(a.roll) +
                                 " probability=" + std::to_string(a.probability));
    d.check(d.faultHits() == 1, "no extra fault hits during retry");
    d.shot("retry-success");
    acknowledge(d);
    d.close();
  } else if (phase == "reload") {
    d.frames();
    d.check(d.screen().page == Page::Home, "reload lands on home");
    d.check(d.save().phase == Phase::EncounterReady && d.save().encounter.id == 2, "next encounter persists after acknowledge");
    d.check(d.save().captureCounts[kThrowSpecies - 1] == 1, "capture persists once");
    d.check(owns(d.save(), kThrowSpecies), "ownership persists");
    d.check(d.save().recentSize >= 1 && d.save().recent[d.save().recentSize - 1].species == kThrowSpecies,
            "capture record persists after restart");
    d.shot("retry-reload");
    d.close();
  }
}

}  // namespace e2e
}  // namespace simulator
