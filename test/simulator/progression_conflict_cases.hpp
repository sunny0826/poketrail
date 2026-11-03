#pragma once
#include "progression_support.hpp"

namespace simulator {
namespace e2e {
namespace progression_conflicts {
using namespace progression_support;
#define REQUIRE(d, cond, msg) do { if (!expect((d), (cond), (msg))) return false; } while (0)

inline bool conflictOwned(Driver& d) {
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

inline bool conflictRetry(Driver& d) {
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

inline bool conflictReload(Driver& d) {
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

#undef REQUIRE
}  // namespace progression_conflicts
}  // namespace e2e
}  // namespace simulator
