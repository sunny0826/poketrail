#include "runtime.hpp"
#include "menu_layout.hpp"
namespace pokedex { namespace ui {
using namespace adventure;
void Runtime::goBack() {
  if (screen_.page == Page::Viewer) {
    navigate(Page::Home); syncPage();
  } else if (screen_.page == Page::Clock && !engine_.view().starter) navigate(Page::DateWelcome);
  else if (screen_.page == Page::EvolutionConfirm) openPartner(screen_.species);
  else if (screen_.page == Page::ReleaseConfirm) navigate(Page::Throw);
  else if (screen_.page == Page::PartnerDetail) navigate(Page::Partners);
  else navigate(Page::Home);
}
void Runtime::handle(const Input& raw) {
  if (screen_.page == Page::Home) {
    if (raw.touch || raw.back) { waitForRelease(); return; }
    if (raw.left || raw.right) navigate(Page::Menu);
    return;
  }
  Input input = raw;
  const bool immediate = screen_.page == Page::Viewer || screen_.page == Page::Battle ||
                         screen_.page == Page::Practice || screen_.page == Page::Throw;
  if (!immediate) {
    if (input.touch) {
      waitForRelease();
      input.left = input.right = false;
    } else {
      const auto action = menuButtons_.update(input.leftDown, input.rightDown);
      input.left = action == MenuAction::Previous || action == MenuAction::Confirm;
      input.right = action == MenuAction::Next || action == MenuAction::Confirm;
    }
  }
  if (!input.any()) return;
  dirty_ = true;
  if (screen_.page == Page::Viewer) {
    if (input.back) goBack();
    return;
  }
  if (screen_.page == Page::Battle || screen_.page == Page::Practice) {
    const bool practice = screen_.page == Page::Practice;
    if (input.pause || (input.touch && input.y < 85)) {
      if (practice) practice_.requestPause(); else engine_.requestPause();
      return;
    }
    bool left = input.left || (input.touch && input.x < 233);
    bool right = input.right || (input.touch && input.x >= 233);
    if (left || right) {
      Action action = left && right ? Action::Both : left ? Action::Dodge : Action::Attack;
      if (practice) practice_.press(action, input.at); else engine_.press(action, input.at);
    }
    return;
  }
  if (screen_.page == Page::Throw) { handleThrow(input); return; }
  if (!input.left && !input.right && !input.touch) return;
  bool confirm = input.left && input.right;
  if (screen_.page == Page::Clock && screen_.editing) {
    if (confirm || (input.touch && input.y >= 365)) screen_.editing = false;
    else editClock(input.left || (input.touch && input.x < 233) ? -1 : 1);
    return;
  }
  const unsigned count = menuCount(screen_, engine_);
  if (!count) return;
  if (screen_.selected >= count) screen_.selected = 0;
  const bool record = screen_.page == Page::FirstCaptures || screen_.page == Page::Recent ||
                      screen_.page == Page::Days || screen_.page == Page::Achievements;
  if (record && input.touch) {
    if (input.y >= 340) { navigate(Page::Records); return; }
    screen_.selected = (screen_.selected + (input.x < 233 ? count - 1 : 1)) % count; return;
  }
  if (input.touch) {
    const int top = screen_.page == Page::Result ? 355 : menu_layout::kTop;
    const int row = menu_layout::rowAt(input.x, input.y, top);
    if (row >= 0) {
      const unsigned selected = menu_layout::first(screen_.selected) + unsigned(row);
      if (selected < count) { screen_.selected = selected; confirm = true; }
    } else if (count > menu_layout::kPageSize && menu_layout::kNextPage.contains(input.x, input.y)) {
      screen_.selected = menu_layout::nextPage(screen_.selected, count);
    }
  } else if (!confirm) {
    screen_.selected = (screen_.selected + (input.left ? count - 1 : 1)) % count;
  }
  if (confirm) activate(screen_.selected, input.at);
}
void Runtime::activate(unsigned item, uint32_t at) {
  const auto& v = engine_.view();
  switch (screen_.page) {
    case Page::Error:
      if (item == 1) navigate(Page::Viewer);
      else {
        if (engine_.savePending()) engine_.retrySave(); else engine_.begin();
        if (!engine_.savePending() && engine_.ready()) {
          batchId_ = v.lastStepBatch > batchId_ ? v.lastStepBatch : batchId_;
          if (pendingClockWrite_) continueClockWrite();
          else if (pendingRoster_ != PendingRoster::None) resumeSavedProgression();
          else navigate(Page::Home);
        }
      }
      break;
    case Page::DateWelcome:
      if (item == 0) openClock();
      else if (item == 1) { dateSkipped_ = true; navigate(v.starter ? Page::Home : Page::Starter); }
      else navigate(Page::Viewer);
      break;
    case Page::Starter:
      if (item == 3) navigate(Page::DateWelcome);
      else { const uint16_t ids[] = {1,4,7}; engine_.chooseStarter(ids[item]); navigate(Page::Home); }
      break;
    case Page::Menu:
      if (item == 0) {
        if (v.phase == Phase::EncounterReady) navigate(Page::Encounter);
        else if (v.phase == Phase::ThrowReady) { deferredThrow_ = 0; navigate(Page::Throw); }
      }
      else if (item == 1) navigate(Page::Routes);
      else if (item == 2) navigate(Page::Partners);
      else if (item == 3) navigate(Page::Viewer);
      else if (item == 4) navigate(Page::Records);
      else if (item == 5) openClock();
      else if (item == 6) { screen_.practice = true; practice_.begin(at); navigate(Page::Practice); }
      else if (item == 7) sleepDisplay();
      else if (item == 8) navigate(Page::Home);
      break;
    case Page::Routes:
      if (item < 3) engine_.selectRoute(Route(item));
      navigate(Page::Home); break;
    case Page::Partners: {
      unsigned row = 0;
      for (unsigned id = 1; id <= 151; ++id) if (owns(v, id)) {
        if (row++ == item) { openPartner(id); return; }
      }
      navigate(v.phase == Phase::EncounterReady ? Page::Encounter : Page::Home); break;
    }
    case Page::PartnerDetail: case Page::EvolutionConfirm: case Page::ReleaseConfirm:
      activateProgression(item, at); break;
    case Page::Encounter:
      if (item == 0) { engine_.startBattle(at); navigate(Page::Battle); }
      else if (item == 1) { screen_.practice = true; practice_.begin(at); navigate(Page::Practice); }
      else if (item == 2) navigate(Page::Partners);
      else navigate(Page::Home);
      break;
    case Page::Pause:
      if (screen_.practice) {
        if (item == 0) { practice_.resume(at); navigate(Page::Practice); }
        else { screen_.practice = false; practice_ = Battle(); navigate(Page::Home); }
      } else if (item == 0) { engine_.resume(at); navigate(Page::Battle); }
      else engine_.retreat();
      break;
    case Page::Result:
      engine_.acknowledgeResult(v.encounter.id, platform_.randomSeed()); navigate(Page::Home); break;
    case Page::Records: {
      const Page pages[] = {Page::FirstCaptures, Page::Recent, Page::Days, Page::Achievements, Page::Home};
      navigate(pages[item]); break;
    }
    case Page::FirstCaptures: case Page::Recent: case Page::Days: case Page::Achievements:
      if (item + 1 == menuCount(screen_, engine_)) navigate(Page::Records);
      break;
    case Page::Clock:
      if (item < 7) screen_.editing = true;
      else if (item == 7) saveClock(at);
      else goBack();
      break;
    default: break;
  }
}
} }
