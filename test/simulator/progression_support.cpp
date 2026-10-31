#include "progression_support.hpp"
#include "viewer_screen.hpp"

namespace simulator {
namespace e2e {
namespace progression_support {
#define REQUIRE(d, cond, msg) do { if (!expect((d), (cond), (msg))) return false; } while (0)

bool expect(Driver& d, bool ok, const std::string& message) {
  d.check(ok, message); return ok;
}
bool wait(Driver& d, const std::function<bool()>& predicate, uint32_t maxMs,
          const std::string& message) {
  d.until(predicate, maxMs, message);
  const bool ok = predicate();
  d.check(ok, message);
  return ok;
}
bool page(Driver& d, Page wanted, const std::string& message, uint32_t maxMs) {
  return wait(d, [&] { return d.screen().page == wanted; }, maxMs, message);
}
bool toHome(Driver& d, const std::string& tag) {
  for (int i = 0; i < 7 && d.screen().page != Page::Home; ++i) {
    if (d.screen().page == Page::Evolution || d.screen().page == Page::EvolutionPreview) {
      d.press(SDL_SCANCODE_RETURN);
      d.until([&] {
        return d.screen().page != Page::Evolution && d.screen().page != Page::EvolutionPreview;
      }, 10000, tag + " finishes evolution animation");
    } else {
      d.back();
      d.frames(2);
    }
  }
  return expect(d, d.screen().page == Page::Home, tag + " returns to home");
}

bool openViewer(Driver& d, const std::string& tag) {
  if (!toHome(d, tag)) return false;
  d.homeMenu();
  REQUIRE(d, page(d, Page::Menu, tag + " opens menu"), tag + " opens menu");
  d.select(3);
  return page(d, Page::Viewer, tag + " opens viewer");
}

bool openPartners(Driver& d, const std::string& tag) {
  if (!toHome(d, tag)) return false;
  d.homeMenu();
  REQUIRE(d, page(d, Page::Menu, tag + " opens menu"), tag + " opens menu");
  d.select(2);
  return page(d, Page::Partners, tag + " opens partner list");
}

bool carry(Driver& d, unsigned row, uint16_t species, const std::string& tag) {
  if (!openPartners(d, tag)) return false;
  d.select(row);
  REQUIRE(d, page(d, Page::PartnerDetail, tag + " opens partner detail"), tag + " opens partner detail");
  REQUIRE(d, d.screen().species == species, tag + " selected expected species");
  d.select(0);
  REQUIRE(d, wait(d, [&] {
            return d.save().partner == species && !d.runtime().engine().savePending();
          }, 10000, tag + " saves carried partner"), tag + " saves carried partner");
  return expect(d, d.save().partner == species, tag + " carries expected species");
}

bool homeEvolution(Driver& d, uint16_t from, uint16_t to, const std::string& tag) {
  if (!toHome(d, tag)) return false;
  REQUIRE(d, wait(d, [&] { return homeEvolutionReady(d.runtime().engine()); }, 3000,
                  tag + " home evolve button becomes available"),
          tag + " home evolve button becomes available");
  d.tap(233, 82);  // centre of kHomeEvolveButton {173, 67, 120, 30}
  REQUIRE(d, page(d, Page::EvolutionConfirm, tag + " opens evolution confirm"), tag + " opens evolution confirm");
  return expect(d, d.screen().evolution.from == from && d.screen().evolution.to == to,
                tag + " confirmation names expected evolution");
}

bool finishEvolution(Driver& d, const std::string& tag) {
  REQUIRE(d, page(d, Page::Evolution, tag + " starts evolution animation", 5000), tag + " starts evolution animation");
  d.press(SDL_SCANCODE_RETURN);
  REQUIRE(d, page(d, Page::PartnerDetail, tag + " evolution animation completes", 10000), tag + " evolution animation completes");
  return expect(d, d.screen().evolved, tag + " reports saved evolution");
}

bool receipt(Driver& d, uint16_t from, uint16_t to, uint64_t cost, const std::string& tag) {
  const Save& s = d.save();
  return expect(d, s.lastEvolution.request.from == from &&
                        s.lastEvolution.request.to == to && s.lastEvolution.cost == cost,
                tag + " persists one evolution receipt");
}

namespace {
bool sameBusiness(const Save& a, const Save& b) {
  if (a.starter != b.starter || a.partner != b.partner || a.route != b.route) return false;
  if (a.owned != b.owned || a.registered != b.registered) return false;
  for (size_t i = 0; i < kSpeciesCount; ++i)
    if (a.progress[i].walked != b.progress[i].walked ||
        a.progress[i].spent != b.progress[i].spent) return false;
  if (a.totalSteps != b.totalSteps || a.explorationCredit != b.explorationCredit ||
      a.explorationSpent != b.explorationSpent || a.encounterTarget != b.encounterTarget ||
      a.undatedSteps != b.undatedSteps || a.stepsIncomplete != b.stepsIncomplete) return false;
  if (a.captureCounts != b.captureCounts || a.recentSize != b.recentSize) return false;
  for (unsigned i = 0; i < a.recentSize; ++i)
    if (a.recent[i].species != b.recent[i].species || a.recent[i].route != b.recent[i].route)
      return false;
  if (a.rosterRevision != b.rosterRevision) return false;
  if (a.lastEvolution.request.from != b.lastEvolution.request.from ||
      a.lastEvolution.request.to != b.lastEvolution.request.to ||
      a.lastEvolution.cost != b.lastEvolution.cost) return false;
  if (a.phase != b.phase || a.result != b.result) return false;
  if (a.encounter.id != b.encounter.id || a.encounter.species != b.encounter.species ||
      a.encounter.partner != b.encounter.partner) return false;
  return a.today.steps == b.today.steps && a.historySize == b.historySize &&
         a.bestDaySteps == b.bestDaySteps;
}
}

bool onlyProgress(const Save& before, const Save& after, uint16_t id, uint64_t total) {
  for (size_t i = 0; i < kSpeciesCount; ++i) {
    const uint64_t delta = after.progress[i].walked - before.progress[i].walked;
    if (i + 1 == id ? delta != total : delta != 0) return false;
  }
  return true;
}

bool sameAccounts(Driver& d, const Save& before, const std::string& tag) {
  const bool ok = sameBusiness(before, d.save());
  if (!ok) d.event("mismatch", tag);
  return expect(d, ok, tag + " leaves evolution, capture, ownership and progress accounts unchanged");
}

// Long viewer navigation can legitimately trigger a periodic checkpoint.
bool quiescentWrites(Driver& d, const std::string& tag) {
  d.frames(4);
  const unsigned before = d.writes();
  d.frames(6);
  return expect(d, d.writes() == before, tag + " idle frames do not write a save slot");
}

void viewerKeys(Driver& d, SDL_Scancode key, unsigned count, const std::string& tag) {
  d.frames();
  for (unsigned i = 0; i < count; ++i) d.press(key);
  d.frames(2);
  d.event("viewer", tag + "=" + std::to_string(pokedex::viewer_screen::species()));
}

bool openPreviewTargets(Driver& d, const std::string& tag) {
  d.frames(2);
  d.tap(233, 423);  // centre of kEvolutionPreviewButton {153, 402, 160, 42}
  return page(d, Page::EvolutionPreviewTargets, tag + " opens evolution targets");
}

bool skipPreviewToViewer(Driver& d, const std::string& tag) {
  d.press(SDL_SCANCODE_RETURN);
  return page(d, Page::Viewer, tag + " preview returns to viewer", 10000);
}

bool practiceRound(Driver& d, const std::string& tag) {
  if (!toHome(d, tag)) return false;
  d.homeMenu();
  REQUIRE(d, page(d, Page::Menu, tag + " opens menu"), tag + " opens menu");
  d.select(5);
  REQUIRE(d, page(d, Page::Practice, tag + " starts practice battle"), tag + " starts practice battle");
  d.tap(233, 35);
  REQUIRE(d, page(d, Page::Pause, tag + " practice pauses"), tag + " practice pauses");
  d.select(0);
  REQUIRE(d, page(d, Page::Practice, tag + " practice resumes"), tag + " practice resumes");
  d.winBattle(true);
  REQUIRE(d, page(d, Page::PracticeThrow, tag + " practice reaches throw page"), tag + " practice reaches throw page");
  d.throwBall(true, true);
  d.finishCapture();
  REQUIRE(d, wait(d, [&] {
            const Page p = d.screen().page;
            return p == Page::PracticeThrow || p == Page::PracticeResult;
          }, 10000, tag + " practice returns from capture animation"),
          tag + " practice returns from capture animation");
  d.shot("isolation-practice");
  if (d.screen().page == Page::PracticeResult) d.select(1); else d.select(2);
  return page(d, Page::Home, tag + " leaves practice");
}

#undef REQUIRE
}
}
}
