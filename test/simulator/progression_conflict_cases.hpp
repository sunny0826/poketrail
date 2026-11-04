#pragma once
#include "progression_support.hpp"

namespace simulator { namespace e2e { namespace progression_conflicts {
using namespace progression_support;
#define REQUIRE(d, cond, msg) do { if (!expect((d), (cond), (msg))) return false; } while (0)

inline bool conflictOwned(Driver& d) {
  const Save start = d.save();
  REQUIRE(d, start.partner == 1 && owns(start, 1) && owns(start, 2) && owns(start, 4), "target-owned fixture has 1 carried and 2 already owned");
  if (!openPartners(d, "target owned")) return false;
  d.select(0);
  REQUIRE(d, page(d, Page::PartnerDetail, "target owned opens species 1 detail"), "target owned opens species 1 detail");
  const Save before = d.save();
  d.select(1);
  d.frames(4);
  REQUIRE(d, d.screen().page == Page::PartnerDetail && d.screen().command == CommandResult::TargetOwned,
          "owning the target rejects the evolution on the detail page");
  if (!sameAccounts(d, before, "target-owned rejection")) return false;
  d.shot("conflict-owned");
  return true;
}

#undef REQUIRE
} } }
