// Progression, failure-retry, isolation and edge scenarios for the desktop E2E driver.
// Only read-only Runtime/Save observations are used; every state change goes through
// SDL input, the real Runtime/Engine and the real FileStore.
#include "progression_support.hpp"
#include "viewer_screen.hpp"

namespace simulator {
namespace e2e {
namespace {
using namespace progression_support;
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

  if (!carry(d, 1, 4, "switch to partner 4")) return false;
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
  if (!homeEvolution(d, 1, 2, "evolve 1 to 2")) return false;
  const Save confirm = d.save();
  d.select(0);
  REQUIRE(d, page(d, Page::Evolution, "evolution confirm submits", 5000), "evolution confirm submits");
  {
    const Save& s = d.save();
    REQUIRE(d, s.progress[0].walked == confirm.progress[0].walked, "evolution does not add progress to the source");
    REQUIRE(d, s.progress[0].spent == confirm.progress[0].spent + 7500, "evolution spends exactly 7500 source steps");
    REQUIRE(d, s.progress[0].walked - s.progress[0].spent == confirm.progress[0].walked - 7500, "source keeps its remaining progress");
    REQUIRE(d, s.progress[1].walked == confirm.progress[1].walked && s.progress[1].spent == 0, "evolution target keeps its own 200 historical steps");
    REQUIRE(d, owns(s, 2) && !owns(s, 1) && owns(s, 4) && registered(s, 1) && registered(s, 2) && s.partner == 2, "evolution replaces the owned form and carries the target");
    REQUIRE(d, receipt(d, 1, 2, 7500, "evolution"), "evolution persists its receipt");
    REQUIRE(d, s.rosterRevision == s.lastEvolution.request.rosterRevision + 1, "evolution advances the roster revision exactly once");
  }
  if (!finishEvolution(d, "evolve 1 to 2")) return false;
  REQUIRE(d, d.screen().species == 2 && d.screen().evolved, "evolution animation ends on the target detail");
  if (!toHome(d, "evolve 1 to 2")) return false;

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
  d.state("progression-end");
  d.shot("progression-end");
  return true;
}

bool progressionReload(Driver& d) {
  d.frames(6);
  REQUIRE(d, page(d, Page::Home, "reloaded progression opens home"), "reloaded progression opens home");
  const Save& s = d.save();
  REQUIRE(d, s.starter == 1 && s.partner == 2, "reload keeps starter 1 and carried species 2");
  REQUIRE(d, owns(s, 2) && !owns(s, 1) && registered(s, 1) && registered(s, 2), "reload keeps the evolved ownership state");
  REQUIRE(d, receipt(d, 1, 2, 7500, "reloaded evolution"), "reload keeps the evolution receipt");
  REQUIRE(d, s.rosterRevision == s.lastEvolution.request.rosterRevision + 1, "reload does not replay the evolution transaction");
  REQUIRE(d, s.progress[0].spent == 7500 && s.progress[0].walked >= 7610 &&
                s.progress[0].walked - s.progress[0].spent >= 110,
          "reload keeps the source debit and remaining progress");
  REQUIRE(d, s.progress[1].spent == 0 && s.progress[1].walked >= 210, "reload keeps the target's own progress and no target debit");
  REQUIRE(d, s.totalSteps >= 7830, "reload keeps all three ten-step walks");
  REQUIRE(d, s.totalSteps == s.explorationSpent + s.explorationCredit, "reload keeps the exploration ledger balanced");
  d.state("progression-reload");
  d.shot("progression-reload");
  return true;
}

bool conflictOwned(Driver& d) {
  const Save start = d.save();
  REQUIRE(d, start.partner == 1 && owns(start, 1) && owns(start, 2) && owns(start, 4), "target-owned fixture has 1 carried and 2 already owned");
  if (!openPartners(d, "target owned")) return false;
  d.select(0);
  REQUIRE(d, page(d, Page::PartnerDetail, "target owned opens species 1 detail"), "target owned opens species 1 detail");
  REQUIRE(d, d.screen().species == 1, "target owned detail is species 1");
  const Save before = d.save();
  d.select(1);
  d.frames(4);
  REQUIRE(d, d.screen().page == Page::PartnerDetail, "owning the target rejects the evolution on the detail page");
  REQUIRE(d, d.screen().command == CommandResult::TargetOwned, "target-owned rejection is reported as TargetOwned");
  if (!sameAccounts(d, before, "target-owned rejection")) return false;
  REQUIRE(d, d.save().progress[0].spent == 0 && d.save().progress[1].walked == 200, "target-owned rejection debits neither account");
  REQUIRE(d, d.save().rosterRevision == before.rosterRevision, "target-owned rejection does not change the roster");
  d.state("conflict-owned");
  d.shot("conflict-owned");
  return true;
}

bool conflictRetry(Driver& d) {
  const Save start = d.save();
  REQUIRE(d, start.partner == 1 && owns(start, 1) && !owns(start, 2), "retry fixture carries 1 and does not own 2");
  if (!openPartners(d, "write-before retry")) return false;
  d.select(0);
  REQUIRE(d, page(d, Page::PartnerDetail, "write-before retry opens detail"), "write-before retry opens detail");
  d.select(1);
  REQUIRE(d, page(d, Page::EvolutionConfirm, "write-before retry opens confirm"), "write-before retry opens confirm");
  const Save before = d.save();
  d.fault(Fault::BeforeWrite);
  d.select(0);
  REQUIRE(d, page(d, Page::Error, "write-before failure opens the error page", 5000), "write-before failure opens the error page");
  REQUIRE(d, d.faultHits() >= 1, "write-before fault was hit");
  REQUIRE(d, d.runtime().engine().savePending(), "failed evolution waits for a retry");
  REQUIRE(d, d.screen().page != Page::Evolution && !d.screen().evolved, "failed evolution does not start the animation");
  if (!sameAccounts(d, before, "write-before failure")) return false;
  REQUIRE(d, d.save().progress[0].spent == 0, "write-before failure does not debit the source");

  d.fault(Fault::None);
  d.select(0);  // error page: retry the same candidate
  REQUIRE(d, page(d, Page::Evolution, "retry commits the same evolution", 10000), "retry commits the same evolution");
  REQUIRE(d, receipt(d, 1, 2, 7500, "retried evolution"), "retry persists one receipt");
  {
    const Save& s = d.save();
    REQUIRE(d, s.progress[0].spent == 7500 && s.progress[0].walked == 7600, "retry debits the source exactly once");
    REQUIRE(d, s.progress[1].walked == 200 && s.progress[1].spent == 0, "retry keeps the target's own progress");
    REQUIRE(d, owns(s, 2) && !owns(s, 1) && s.partner == 2 &&
                  s.rosterRevision == s.lastEvolution.request.rosterRevision + 1,
            "retry applies one complete evolution result");
  }
  if (!finishEvolution(d, "write-before retry")) return false;
  d.state("conflict-retry");
  d.shot("conflict-retry");
  return true;
}

bool conflictReload(Driver& d) {
  d.frames(6);
  REQUIRE(d, page(d, Page::Home, "reloaded retry opens home"), "reloaded retry opens home");
  const Save& s = d.save();
  REQUIRE(d, receipt(d, 1, 2, 7500, "reloaded retry"), "reload keeps the retried receipt");
  REQUIRE(d, s.progress[0].spent == 7500 && s.progress[0].walked == 7600, "reload keeps one 7500-step debit");
  REQUIRE(d, s.progress[1].walked == 200 && s.progress[1].spent == 0, "reload keeps the target account untouched");
  REQUIRE(d, owns(s, 2) && !owns(s, 1) && s.partner == 2, "reload keeps the retried ownership result");
  REQUIRE(d, s.rosterRevision == s.lastEvolution.request.rosterRevision + 1, "reload sees one committed evolution, not a replay");
  d.state("conflict-reload");
  d.shot("conflict-reload");
  return true;
}

bool isolationRun(Driver& d) {
  const Save before = d.save();
  REQUIRE(d, before.starter == 1 && before.partner == 1, "isolation fixture starts on carried species 1");
  if (!quiescentWrites(d, "isolation")) return false;

  if (!openViewer(d, "isolated preview 1")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 1, "viewer starts on species 1");
  const unsigned writesBeforePreview = d.writes();
  d.tap(233, 423);
  REQUIRE(d, page(d, Page::EvolutionPreview, "species 1 preview starts"), "species 1 preview starts");
  REQUIRE(d, d.screen().evolutionAnim.from == 1 && d.screen().evolutionAnim.to == 2, "species 1 preview names 1 to 2");
  if (!skipPreviewToViewer(d, "species 1")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 1, "preview returns to the same viewer species");
  REQUIRE(d, d.writes() == writesBeforePreview, "species 1 preview writes no save slot");
  if (!sameAccounts(d, before, "species 1 preview")) return false;

  viewerKeys(d, SDL_SCANCODE_RIGHT, 132, "viewer to 133");
  REQUIRE(d, pokedex::viewer_screen::species() == 133, "viewer reaches Eevee 133");
  const unsigned writesBeforeEevee = d.writes();
  if (!openPreviewTargets(d, "Eevee 133")) return false;
  REQUIRE(d, d.screen().evolutionAnim.from == 133, "Eevee preview names species 133");
  REQUIRE(d, menuCount(d.screen(), d.runtime().engine()) == 8, "Eevee preview offers seven branches plus return");
  d.select(0);
  REQUIRE(d, page(d, Page::EvolutionPreview, "Eevee first branch preview starts"), "Eevee first branch preview starts");
  REQUIRE(d, d.screen().evolutionAnim.to == 134, "Eevee first branch previews 134");
  if (!skipPreviewToViewer(d, "Eevee 133")) return false;
  REQUIRE(d, pokedex::viewer_screen::species() == 133, "Eevee preview returns to species 133");
  REQUIRE(d, d.writes() == writesBeforeEevee, "Eevee preview writes no save slot");
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
  d.swipe(350, 100);
  REQUIRE(d, d.screen().selected == 3, "edges partner list reaches its last page");
  d.shot("edges-partners-last");
  d.select(5);
  REQUIRE(d, page(d, Page::Menu, "last partner page returns to menu"), "last partner page returns to menu");

  if (!openViewer(d, "edges viewer")) return false;
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
  d.check(d.closeErrors() == 0, "progression " + phase + " closes cleanly");
}

void scenarioConflict(Driver& d, const std::string& phase) {
  d.frames(4);
  bool ok = false;
  if (phase == "owned") ok = conflictOwned(d);
  else if (phase == "retry") ok = conflictRetry(d);
  else if (phase == "reload") ok = conflictReload(d);
  else {
    d.check(false, "unknown conflict phase: " + phase);
    return;
  }
  if (!ok) {
    d.state("conflict-failed");
    return;
  }
  d.close();
  d.check(d.closeErrors() == 0, "conflict " + phase + " closes cleanly");
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
  d.check(d.closeErrors() == 0, "isolation run closes cleanly");
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
  d.check(d.closeErrors() == 0, "edges run closes cleanly");
}

#undef REQUIRE
}  // namespace e2e
}  // namespace simulator
