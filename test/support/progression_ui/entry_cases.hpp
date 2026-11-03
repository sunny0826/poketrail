#pragma once
#include "../progression_ui.hpp"
#include "../rendered_text.hpp"

namespace progression_ui {
void carriedPartner() {
  MemoryStore store; legacyOwned(store, {1, 4});
  { Engine e(store); e.begin(); step(e, 1, 7600); e.checkpoint(); }
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  openMenu(runtime); select(runtime, 3);
  check(runtime.screen().page == Page::CarriedPartner, "menu opens carried partner directly");
  screenshot(runtime, "carried-ready");
  check(glyph("陪伴 7600 · 可用 7600 步", 80, theme::kMint), "carried page renders evolution view balances");
  check(rendered_text::contains("7500 步", theme::kText), "target row includes its cost");
  select(runtime, 1); check(runtime.screen().page == Page::EvolutionConfirm, "carried target requires confirmation");
  select(runtime, 1); check(runtime.screen().page == Page::CarriedPartner, "later returns to carried page");
  select(runtime, 1); tap(runtime, 233, 30);
  check(runtime.screen().page == Page::CarriedPartner, "confirm top back returns to carried page");
  select(runtime, 0); select(runtime, 1);
  store.fail = true; select(runtime, 0);
  check(runtime.screen().page == Page::Error && runtime.engine().view().partner == 1,
        "failed switch keeps the old published partner");
  store.fail = false; select(runtime, 0);
  check(runtime.screen().page == Page::PartnerDetail && runtime.engine().view().partner == 4,
        "switch retry commits through existing partner detail");
  tap(runtime, 233, 30); tap(runtime, 233, 30);
  check(runtime.screen().page == Page::CarriedPartner && runtime.screen().species == 4,
        "return from partner list refreshes the carried species");
  screenshot(runtime, "carried-switched");
  check(glyph("陪伴 0 · 可用 0 步", 80, theme::kMint) &&
        glyph("培养步数还不够", 188, theme::kGold), "new carrier uses its own account and reason");
  select(runtime, 1);
  check(runtime.screen().page == Page::CarriedPartner && runtime.screen().command == CommandResult::InsufficientProgress,
        "insufficient progress cannot open confirmation");
  select(runtime, 2);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3,
        "carried return restores menu selection");
  select(runtime, 3); tap(runtime, 233, 30);
  check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3,
        "top back also restores menu selection");
  // Re-entering the ordinary list must discard the carried-page return route.
  select(runtime, 2); tap(runtime, 233, 30);
  check(runtime.screen().page == Page::Menu, "ordinary partner list still returns to menu");
  select(runtime, 3); select(runtime, 0); select(runtime, 0); select(runtime, 0);
  tap(runtime, 233, 30); tap(runtime, 233, 30); select(runtime, 1);
  store.fail = true; select(runtime, 0);
  check(runtime.screen().page == Page::Error && runtime.engine().view().progress[0].spent == 0,
        "failed carried evolution publishes neither reward nor debit");
  store.fail = false; select(runtime, 0); idle(runtime, kEvolutionTotalMs);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2 &&
        runtime.engine().view().progress[0].spent == 7500, "retried evolution ends on existing target detail once");
  tap(runtime, 233, 30); tap(runtime, 233, 30);
  screenshot(runtime, "carried-evolved");
  check(runtime.screen().page == Page::CarriedPartner && runtime.screen().species == 2 &&
        glyph("陪伴 0 · 可用 0 步", 80, theme::kMint), "evolved carrier displays target account");
}
void carriedPartnerEdges() {
  HostPlatform platform;
  MemoryStore store; modernOwned(store, {1, 133, 134}, 133);
  { Engine e(store); e.begin(); step(e, 133, 10001); e.selectLanguage(pokedex::i18n::Language::English); e.checkpoint(); }
  Runtime runtime(store, platform); start(runtime); openMenu(runtime); select(runtime, 3);
  screenshot(runtime, "carried-eevee-owned-english");
  check(rendered_text::contains("You already own this form", theme::kGold), "owned target reason is visible");
  select(runtime, 1);
  check(runtime.screen().page == Page::CarriedPartner && runtime.screen().command == CommandResult::TargetOwned,
        "owned branch cannot open confirmation");
  edge(runtime, false, true);
  check(runtime.screen().selected == 2 && glyph("Jolteon needs 10000 steps", 188, theme::kGold) &&
        !rendered_text::contains("You already own this form", theme::kGold),
        "moving to an available branch replaces the previous rejection with its current cost");
  screenshot(runtime, "carried-eevee-ready-after-rejection");
  select(runtime, 7);
  check(runtime.screen().page == Page::EvolutionConfirm && runtime.screen().evolution.to == 471,
        "last carried Eevee branch is reachable");
  select(runtime, 1); swipe(runtime, 300, 300, 160, 300); swipe(runtime, 300, 300, 160, 300);
  screenshot(runtime, "carried-eevee-last-english");
  select(runtime, 8); check(runtime.screen().page == Page::Menu && runtime.screen().selected == 3,
                          "last carried page has a working return");
  MemoryStore finalStore; modernOwned(finalStore, {1, 160}, 160);
  { Engine e(finalStore); e.begin(); e.selectLanguage(pokedex::i18n::Language::English); }
  Runtime finalForm(finalStore, platform); start(finalForm); openMenu(finalForm); select(finalForm, 3);
  screenshot(finalForm, "carried-final-long-english");
  check(rendered_text::contains("Feraligatr", theme::kText) &&
        glyph("Already in final form", 188, theme::kMuted), "long English name and final form fit");
  check(menuCount(finalForm.screen(), finalForm.engine()) == 2, "final form has switch and return only");
  // Runtime keeps a new save in onboarding; draw its defensive empty state directly.
  MemoryStore empty; Engine engine(empty); engine.begin(); Screens screens; check(screens.begin(), "empty renderer starts");
  ScreenState state; state.page = Page::CarriedPartner;
  check(screens.draw(state, engine, {}, 0) && menuCount(state, engine) == 1 &&
        glyph("尚未携带伙伴", 150, theme::kMuted), "empty page offers only return, not a final-form claim");
  // Disallowed phases normally route to battle/throw; their read-only view stays unavailable.
  Engine blocked(store); blocked.begin();
  check(blocked.startBattle(100) == CommandResult::Applied, "phase fixture enters battle");
  state.language = pokedex::i18n::Language::English; state.selected = 2;
  screens.draw(state, blocked, {}, 100);
  check(blocked.evolutionView(133).options[1].status == CommandResult::InvalidPhase &&
        glyph("Carry it and finish this encounter", 188, theme::kGold), "phase restriction is rendered from evolution view");
}
void starterWith(MemoryStore& store, unsigned steps) {
  Engine engine(store); check(engine.begin() == CommandResult::Applied, "new engine boots");
  engine.chooseStarter(1); step(engine, 1, steps); engine.checkpoint();
}
void back(Runtime& runtime) { tap(runtime, 233, 30); }
void homeEvolveButton() {
  const auto& b = kHomeEvolveButton;
  const int bx = b.x + b.width / 2, by = b.y + b.height / 2;
  HostPlatform platform;
  MemoryStore early; starterWith(early, 7499);
  Runtime waiting(early, platform); start(waiting);
  check(!homeEvolutionReady(waiting.engine()) && glyph("携带中", 80, theme::routeColor(Route::Kanto)) && !glyph("进化", by, theme::kGold),
        "one step short keeps the carried label and hides the evolve button");
  tap(waiting, bx, by);
  check(waiting.screen().page == Page::Menu, "button area still opens the menu while evolution is unavailable");

  MemoryStore store; starterWith(store, 7500);
  Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::Home && glyph("进化", by, theme::kGold) && !glyph("携带中", 80, theme::routeColor(Route::Kanto)),
        "reaching the cost shows the evolve button on home");
  screenshot(runtime, "home-evolve-ready");
  const unsigned commits = store.commits;
  tap(runtime, bx, by);
  check(runtime.screen().page == Page::EvolutionConfirm && runtime.screen().evolution.from == 1 &&
        runtime.screen().evolution.to == 2 && store.commits == commits,
        "home button opens the existing confirmation without saving");
  select(runtime, 1);
  check(runtime.screen().page == Page::Home && runtime.engine().view().progress[0].spent == 0,
        "later from a home evolution returns home and keeps progress");
  tap(runtime, bx, by); back(runtime);
  check(runtime.screen().page == Page::Home, "top back from a home evolution returns home");
  tap(runtime, b.x + 4, by); select(runtime, 0);
  check(runtime.screen().page == Page::Evolution && runtime.screen().evolutionAnim.to == 2,
        "confirming from home plays the saved evolution");
  idle(runtime, kEvolutionTotalMs);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2 &&
        runtime.engine().view().partner == 2 && runtime.engine().view().progress[0].spent == 7500,
        "home evolution finishes on target detail with the normal charge");
  back(runtime); back(runtime); back(runtime);
  check(runtime.screen().page == Page::Home && !homeEvolutionReady(runtime.engine()) &&
        glyph("携带中", 80, theme::routeColor(Route::Kanto)), "new form without steps hides the button again");

  MemoryStore owned; legacyOwned(owned, {1, 2});
  { Engine e(owned); e.begin(); step(e, 1, 7500); e.checkpoint(); }
  Runtime blocked(owned, platform); start(blocked);
  check(!homeEvolutionReady(blocked.engine()) && !glyph("进化", by, theme::kGold),
        "owned target hides the button");
  MemoryStore finalForm; legacyOwned(finalForm, {1, 3}, 3);
  { Engine e(finalForm); e.begin(); step(e, 3, 20000); e.checkpoint(); }
  Runtime last(finalForm, platform); start(last);
  check(!homeEvolutionReady(last.engine()) && !glyph("进化", by, theme::kGold), "final form hides the button");

  MemoryStore eevee; legacyOwned(eevee, {1, 133, 134}, 133);
  { Engine e(eevee); e.begin(); step(e, 133, 10000); e.checkpoint(); }
  Runtime branches(eevee, platform); start(branches);
  check(glyph("进化", by, theme::kGold), "Eevee with an open branch shows one button");
  tap(branches, bx, by);
  check(branches.screen().page == Page::PartnerDetail && branches.screen().species == 133 &&
        branches.screen().selected == 2, "multiple targets open the branch list at the first open branch");
  screenshot(branches, "home-evolve-branches");
}
}  // namespace progression_ui
