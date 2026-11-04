// Progression, failure-retry, isolation and edge scenarios for the desktop E2E driver.
// Only read-only Runtime/Save observations are used; every state change goes through
// SDL input, the real Runtime/Engine and the real FileStore.
#include "progression_support.hpp"
#include "progression_conflict_cases.hpp"
#include "viewer_screen.hpp"
#include "capture_badge_checks.hpp"
#include "../support/rendered_text.hpp"

namespace simulator {
namespace e2e {
namespace {
using namespace progression_support;
using namespace progression_conflicts;
#define REQUIRE(d, cond, msg) do { if (!expect((d), (cond), (msg))) return false; } while (0)

bool progressionRun(Driver& d) {
  const Save start = d.save();
  REQUIRE(d, start.starter == 1 && start.partner == 1 && start.totalSteps == 7800, "evolution fixture starts with starter 1 at 7800 steps");
  REQUIRE(d, owns(start, 1) && owns(start, 4) && !owns(start, 2) && registered(start, 2), "evolution fixture owns 1 and 4, and has only registered 2");
  REQUIRE(d, start.progress[0].walked == 7600 && start.progress[1].walked == 200, "evolution fixture carries the 7600/200 progress split");
  d.state("progression-start");

  const uint64_t baseTotal = start.totalSteps;
  d.walk(10);
  REQUIRE(d, wait(d, [&] {
            return d.save().totalSteps >= baseTotal + 10 && !d.runtime().engine().savePending();
          }, 30000, "first ten steps reach the engine"), "first ten steps reach the engine");
  {
    const Save& s = d.save();
    const uint64_t total = s.totalSteps - baseTotal;
    REQUIRE(d, total >= 10, "first walk really produced steps");
    REQUIRE(d, onlyProgress(start, s, 1, total), "first walk credits only carried species 1");
  }

  if (!homeEvolution(d, 1, 2, "home evolution entry")) return false;
  d.select(1);
  REQUIRE(d, page(d, Page::Home, "home evolution cancellation returns home") &&
                d.save().progress[0].spent == 0, "home evolution cancellation leaves steps intact");
  if (!toHome(d, "carried partner")) return false;
  d.homeMenu(); d.select(3);
  REQUIRE(d, page(d, Page::CarriedPartner, "menu opens carried partner"), "menu opens carried partner");
  REQUIRE(d, d.screen().species == 1, "carried page reads the current partner");
  d.frames();
  const std::string steps = "陪伴 " + std::to_string(d.save().progress[0].walked) + " · 可用 " +
                            std::to_string(d.save().progress[0].walked - d.save().progress[0].spent) + " 步";
  REQUIRE(d, rendered_text::contains("妙蛙种子", theme::kText) &&
                rendered_text::contains(steps.c_str(), theme::kMint) &&
                rendered_text::contains("7500 步", theme::kText) &&
                rendered_text::contains("切换携带者", theme::kMint),
          "carried page renders species, account, evolution cost and switch action");
  d.shot("carried-ready");
  d.select(1);
  REQUIRE(d, page(d, Page::EvolutionConfirm, "carried target opens confirmation"), "carried target opens confirmation");
  d.select(1);
  REQUIRE(d, page(d, Page::CarriedPartner, "later returns to carrier"), "later returns to carrier");
  d.select(0);
  REQUIRE(d, page(d, Page::Partners, "switch opens existing list"), "switch opens existing list");
  d.select(1); d.fault(Fault::BeforeWrite); d.select(0);
  REQUIRE(d, page(d, Page::Error, "switch failure opens retry") && d.save().partner == 1,
          "failed switch retains the published carrier");
  d.fault(Fault::None); d.select(0);
  REQUIRE(d, page(d, Page::PartnerDetail, "switch retry returns to detail") && d.save().partner == 4,
          "existing detail saves switched carrier after retry");
  d.back(); d.back();
  REQUIRE(d, page(d, Page::CarriedPartner, "list returns to carried page") && d.screen().species == 4,
          "list returns to carried page with new carrier");
  d.shot("carried-switched");
  d.back();
  REQUIRE(d, d.screen().page == Page::Menu && d.screen().selected == 3, "carried top back restores menu selection");
  if (!toHome(d, "switch to partner 4")) return false;
  const Save afterSwitch = d.save();
  const uint64_t baseTotal4 = afterSwitch.totalSteps;
  d.walk(10);
  REQUIRE(d, wait(d, [&] {
            return d.save().totalSteps >= baseTotal4 + 10 && !d.runtime().engine().savePending();
          }, 30000, "ten steps with partner 4 reach the engine"),
          "ten steps with partner 4 reach the engine");
  {
    const Save& s = d.save();
    const uint64_t total = s.totalSteps - baseTotal4;
    REQUIRE(d, onlyProgress(afterSwitch, s, 4, total), "second walk credits only carried species 4");
  }

  if (!carry(d, 0, 1, "switch back to partner 1")) return false;
  if (!toHome(d, "carried evolution")) return false;
  d.homeMenu(); d.select(3); d.select(1);
  REQUIRE(d, page(d, Page::EvolutionConfirm, "carried evolution opens confirmation"),
          "carried evolution opens confirmation");
  const Save confirm = d.save();
  d.fault(Fault::BeforeWrite); d.select(0);
  REQUIRE(d, page(d, Page::Error, "carried evolution failure opens retry") &&
                d.save().partner == 1 && d.save().progress[0].spent == confirm.progress[0].spent,
          "failed carried evolution publishes no target or debit");
  // A free preview must not replace the real evolution waiting for a save retry.
  d.select(1);
  REQUIRE(d, page(d, Page::Viewer, "save error still allows the free viewer") &&
                pokedex::viewer_screen::species() == 1, "save error opens the free viewer on species 1");
  viewerKeys(d, SDL_SCANCODE_RIGHT, 3, "pending evolution preview to species 4");
  REQUIRE(d, pokedex::viewer_screen::species() == 4, "pending evolution can preview a different source");
  d.tap(233, 423);
  REQUIRE(d, page(d, Page::EvolutionPreview, "pending evolution allows a free preview") &&
                d.screen().evolutionAnim.from == 4 && d.screen().evolutionAnim.to == 5,
          "free preview plays 4 to 5 while real evolution 1 to 2 awaits retry");
  if (!skipPreviewToViewer(d, "pending evolution preview 4 to 5")) return false;
  d.back();
  REQUIRE(d, page(d, Page::Error, "leaving the free viewer restores the save error") &&
                d.runtime().engine().savePending(), "original save still requires retry after the preview");
  if (!sameAccounts(d, confirm, "free preview during failed evolution")) return false;
  d.shot("progression-preview-before-retry");
  d.fault(Fault::None); d.select(0);
  REQUIRE(d, page(d, Page::Evolution, "evolution retry submits", 5000) &&
                d.screen().evolutionAnim.from == 1 && d.screen().evolutionAnim.to == 2,
          "retry resumes the original 1 to 2 evolution instead of the free preview");
  {
    const Save& s = d.save();
    REQUIRE(d, s.progress[0].walked == confirm.progress[0].walked, "evolution does not add progress to the source");
    REQUIRE(d, s.progress[0].spent == confirm.progress[0].spent + 7500, "evolution spends exactly 7500 source steps");
    REQUIRE(d, s.progress[1].walked == confirm.progress[1].walked && s.progress[1].spent == 0, "evolution target keeps its own 200 historical steps");
    REQUIRE(d, s.progress[3].walked == confirm.progress[3].walked && s.progress[3].spent == confirm.progress[3].spent &&
                  s.progress[4].walked == confirm.progress[4].walked && s.progress[4].spent == confirm.progress[4].spent &&
                  !owns(s, 5) && !registered(s, 5),
            "retry leaves the preview source and target accounts and ownership unchanged");
    REQUIRE(d, owns(s, 2) && !owns(s, 1) && owns(s, 4) && registered(s, 1) && registered(s, 2) && s.partner == 2, "evolution replaces the owned form and carries the target");
  }
  if (!finishEvolution(d, "evolve 1 to 2")) return false;
  REQUIRE(d, d.screen().species == 2 && d.screen().evolved, "evolution animation ends on the target detail");
  if (!toHome(d, "evolve 1 to 2")) return false;

  checkCaptureBadge(d, 0, "evolution-no-capture");
  const Save afterEvolution = d.save();
  const uint64_t baseTotal2 = afterEvolution.totalSteps;
  d.walk(10);
  REQUIRE(d, wait(d, [&] {
            return d.save().totalSteps >= baseTotal2 + 10 && !d.runtime().engine().savePending();
          }, 30000, "ten steps with evolved partner 2 reach the engine"),
          "ten steps with evolved partner 2 reach the engine");
  {
    const Save& s = d.save();
    const uint64_t total = s.totalSteps - baseTotal2;
    REQUIRE(d, onlyProgress(afterEvolution, s, 2, total), "post-evolution walk credits only species 2");
    REQUIRE(d, s.progress[0].spent == afterEvolution.progress[0].spent,
            "post-evolution walk leaves the source debit untouched");
  }
  d.homeMenu(); d.select(3);
  REQUIRE(d, page(d, Page::CarriedPartner, "evolved carrier can be reopened") && d.screen().species == 2,
          "evolved carrier can be reopened");
  d.shot("carried-before-reload");
  d.state("progression-end");
  return true;
}

bool progressionReload(Driver& d) {
  d.frames(6);
  REQUIRE(d, page(d, Page::Home, "reloaded progression opens home"), "reloaded progression opens home");
  const Save& s = d.save();
  REQUIRE(d, s.starter == 1 && s.partner == 2, "reload keeps starter 1 and carried species 2");
  REQUIRE(d, owns(s, 2) && !owns(s, 1) && registered(s, 1) && registered(s, 2), "reload keeps the evolved ownership state");
  REQUIRE(d, s.progress[0].spent == 7500 && s.progress[0].walked >= 7610 &&
                s.progress[0].walked - s.progress[0].spent >= 110,
          "reload keeps the source debit and remaining progress");
  REQUIRE(d, s.progress[1].spent == 0 && s.progress[1].walked >= 210, "reload keeps the target's own progress and no target debit");
  REQUIRE(d, s.totalSteps >= 7830, "reload keeps all three ten-step walks");
  REQUIRE(d, s.totalSteps == s.explorationSpent + s.explorationCredit, "reload keeps the exploration ledger balanced");
  checkCaptureBadge(d, 0, "evolution-reloaded-no-capture");
  d.homeMenu(); d.select(3);
  REQUIRE(d, page(d, Page::CarriedPartner, "reload reopens carried page") && d.screen().species == 2,
          "reload reopens the same carried species");
  d.shot("carried-after-reload");
  d.state("progression-reload");
  return true;
}

bool isolationRun(Driver& d) {
  const Save before = d.save();
  REQUIRE(d, before.starter == 1 && before.partner == 1, "isolation fixture starts on carried species 1");

  if (!openViewer(d, "isolated preview 1")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 1, "viewer starts on species 1");
  d.tap(233, 423);
  REQUIRE(d, page(d, Page::EvolutionPreview, "species 1 preview starts"), "species 1 preview starts");
  REQUIRE(d, d.screen().evolutionAnim.from == 1 && d.screen().evolutionAnim.to == 2, "species 1 preview names 1 to 2");
  if (!skipPreviewToViewer(d, "species 1")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 1, "preview returns to the same viewer species");
  if (!sameAccounts(d, before, "species 1 preview")) return false;

  viewerKeys(d, SDL_SCANCODE_RIGHT, 132, "viewer to 133");
  REQUIRE(d, pokedex::viewer_screen::species() == 133, "viewer reaches Eevee 133");
  if (!openPreviewTargets(d, "Eevee 133")) return false;
  REQUIRE(d, d.screen().evolutionAnim.from == 133, "Eevee preview names species 133");
  REQUIRE(d, menuCount(d.screen(), d.runtime().engine()) == 8, "Eevee preview offers seven branches plus return");
  d.select(0);
  REQUIRE(d, page(d, Page::EvolutionPreview, "Eevee first branch preview starts"), "Eevee first branch preview starts");
  REQUIRE(d, d.screen().evolutionAnim.to == 134, "Eevee first branch previews 134");
  if (!skipPreviewToViewer(d, "Eevee 133")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 133, "Eevee preview returns to species 133");
  if (!sameAccounts(d, before, "Eevee preview")) return false;
  if (!toHome(d, "isolated previews")) return false;

  if (!practiceRound(d, "isolated practice")) return false;
  if (!sameAccounts(d, before, "practice battle, pause, throw and return")) return false;
  d.state("isolation-end");
  return true;
}

bool edgesRun(Driver& d) {
  const Save start = d.save();
  REQUIRE(d, owns(start, 1) && owns(start, 4) && owns(start, 133) && owns(start, 439) && owns(start, 649) &&
                registered(start, 1) && registered(start, 4) && registered(start, 133) &&
                registered(start, 439) && registered(start, 649),
          "edges fixture owns and has registered 1, 4, 133, 439 and 649");
  if (!openPartners(d, "edges partners")) return false;
  REQUIRE(d, menuCount(d.screen(), d.runtime().engine()) == 6, "edges partner list has five owners and return");
  d.shot("edges-partners-first");
  // Fixed user coordinates: outside the third row must be inert; its visible edge must work.
  d.tap(90, 374); d.tap(376, 374);
  REQUIRE(d, d.screen().page == Page::Partners && d.screen().selected == 0,
          "touch outside the third partner row is inert");
  d.tap(96, 374);
  REQUIRE(d, page(d, Page::PartnerDetail, "third row edge opens its partner") && d.screen().species == 133,
          "touching the third row edge opens the correct partner");
  d.back();
  d.tap(330, 425);
  REQUIRE(d, d.screen().selected == 3, "enlarged next-page zone reaches the last partner page");
  d.frames();
  d.shot("edges-partners-last");
  d.select(5);
  REQUIRE(d, page(d, Page::Menu, "last partner page returns to menu"), "last partner page returns to menu");

  if (!openViewer(d, "edges viewer")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 1, "viewer touch navigation starts on species 1");
  d.swipe(340, 120);
  REQUIRE(d, d.screen().page == Page::Viewer && pokedex::viewer_screen::species() == 2,
          "left swipe advances the viewer to species 2");
  d.swipe(120, 340);
  REQUIRE(d, d.screen().page == Page::Viewer && pokedex::viewer_screen::species() == 1,
          "right swipe returns the viewer to species 1");
  d.swipe(233, 257);
  REQUIRE(d, d.screen().page == Page::Viewer && pokedex::viewer_screen::species() == 1,
          "short drag keeps the current viewer species");
  viewerKeys(d, SDL_SCANCODE_LEFT, 211, "viewer to 439 via the list end");
  REQUIRE(d, pokedex::viewer_screen::species() == 439, "viewer reaches long-name species 439");
  d.shot("edges-viewer-439");
  viewerKeys(d, SDL_SCANCODE_LEFT, 306, "viewer to 133");
  REQUIRE(d, pokedex::viewer_screen::species() == 133, "viewer reaches Eevee 133");

  if (!openPreviewTargets(d, "edges Eevee")) return false;
  d.swipe(350, 100);
  d.swipe(350, 100);
  REQUIRE(d, d.screen().selected == 6, "Eevee preview reaches its last page");
  d.shot("edges-eevee-last");
  d.select(6);
  REQUIRE(d, page(d, Page::EvolutionPreview, "last Eevee branch preview starts"), "last Eevee branch preview starts");
  REQUIRE(d, d.screen().evolutionAnim.from == 133 && d.screen().evolutionAnim.to == 471, "last Eevee branch previews 471");
  if (!skipPreviewToViewer(d, "edges Eevee branch")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 133, "Eevee branch preview returns to 133");

  if (!openPreviewTargets(d, "edges Eevee return")) return false;
  d.swipe(350, 100);
  d.swipe(350, 100);
  d.select(7);
  REQUIRE(d, page(d, Page::Viewer, "Eevee target list return goes back to viewer"), "Eevee target list return goes back to viewer");
  REQUIRE(d, pokedex::viewer_screen::species() == 133, "Eevee target list return keeps the viewer species");
  d.shot("edges-preview-return");
  if (!toHome(d, "edges")) return false;
  d.state("edges-end");
  return true;
}

}  // namespace

void scenarioProgression(Driver& d, const std::string& phase) {
  d.frames(4);
  bool ok = false;
  if (phase == "run") ok = progressionRun(d);
  else if (phase == "reload") ok = progressionReload(d);
  else {
    d.check(false, "unknown progression phase: " + phase);
    return;
  }
  if (!ok) {
    d.state("progression-failed");
    return;
  }
  d.close();
}

void scenarioConflict(Driver& d, const std::string& phase) {
  d.frames(4);
  bool ok = false;
  if (phase == "owned") ok = conflictOwned(d);
  else {
    d.check(false, "unknown conflict phase: " + phase);
    return;
  }
  if (!ok) {
    d.state("conflict-failed");
    return;
  }
  d.close();
}

void scenarioIsolation(Driver& d, const std::string& phase) {
  d.frames(4);
  if (phase != "run") {
    d.check(false, "unknown isolation phase: " + phase);
    return;
  }
  if (!isolationRun(d)) {
    d.state("isolation-failed");
    return;
  }
  d.close();
}

void scenarioEdges(Driver& d, const std::string& phase) {
  d.frames(4);
  if (phase != "run") {
    d.check(false, "unknown edges phase: " + phase);
    return;
  }
  if (!edgesRun(d)) {
    d.state("edges-failed");
    return;
  }
  d.close();
}

#undef REQUIRE
}  // namespace e2e
}  // namespace simulator
