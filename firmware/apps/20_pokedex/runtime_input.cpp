#include "runtime.hpp"
#include "menu_layout.hpp"
#include "viewer_screen.hpp"
namespace pokedex { namespace ui {
using namespace adventure;
void Runtime::goBack() {
  if (screen_.page == Page::PracticeThrow || screen_.page == Page::PracticeResult) endPractice();
  else if (screen_.page == Page::Viewer) {
    navigate(Page::Home); syncPage();
  } else if (screen_.page == Page::Resources) navigate(Page::Menu);
  else if (screen_.page == Page::Records) navigate(Page::Resources);
  else if (screen_.page == Page::FirstCaptures || screen_.page == Page::Recent ||
           screen_.page == Page::Days || screen_.page == Page::Achievements) navigate(Page::Records);
  else if (screen_.page == Page::Clock)
    navigate(engine_.view().starter ? Page::Resources : Page::DateWelcome);
  else if (screen_.page == Page::EvolutionPreviewTargets) navigate(Page::Viewer);
  else if (screen_.page == Page::EvolutionConfirm) openPartner(screen_.species);
  else if (screen_.page == Page::ReleaseConfirm) navigate(Page::Throw);
  else if (screen_.page == Page::PartnerDetail) navigate(Page::Partners);
  else if (screen_.page == Page::Partners || screen_.page == Page::Routes) navigate(Page::Menu);
  else navigate(Page::Home);
}
void Runtime::handle(const Input& raw) {
  if (screen_.page == Page::Evolution || screen_.page == Page::EvolutionPreview) {
    if (raw.left || raw.right || raw.click) {
      skipEvolution(screen_.evolutionAnim, raw.at);
      dirty_ = true;
      waitForRelease();
    }
    return;
  }
  if (screen_.page == Page::Home) {
    if (raw.touch) { navigate(Page::Menu); return; }
    if (raw.back) { waitForRelease(); return; }
    if (raw.left || raw.right) navigate(Page::Menu);
    return;
  }
  Input input = raw;
  const bool immediate = screen_.page == Page::Viewer || screen_.page == Page::Battle ||
                         screen_.page == Page::Practice || screen_.page == Page::Throw ||
                         screen_.page == Page::PracticeThrow;
  if (!immediate) {
    if (input.touch || input.click || input.swipe) {
      menuButtons_.consume();
      input.left = input.right = false;
    } else {
      const auto action = menuButtons_.update(input.leftDown, input.rightDown);
      input.left = action == MenuAction::Previous || action == MenuAction::Confirm;
      input.right = action == MenuAction::Next || action == MenuAction::Confirm;
    }
  }
  const bool throwRelease = !input.held && !input.rightDown &&
    ((screen_.page == Page::Throw && screen_.charging && engine_.throwView().active) ||
     (screen_.page == Page::PracticeThrow && screen_.charging && practiceCapture_.throwView().active));
  if (!input.any() || throwRelease) {
    if (screen_.page == Page::Throw && screen_.charging && engine_.throwView().active &&
        !input.held && !input.rightDown) {
      screen_.charging = false;
      const auto result = engine_.pressThrow(engine_.throwView().key, input.at, readingAt(input.at));
      if (result == CommandResult::Applied) {
        const auto& key = engine_.throwView().key;
        const auto& a = engine_.view().encounter.attempts[key.attemptIndex - 1];
        CaptureAnim anim;
        anim.active = true;
        anim.startMs = input.at;
        anim.timing = a.timing;
        anim.captured = a.captured;
        anim.wiggleBreak = a.captured ? 0 : uint8_t(1 + a.roll % 3);
        screen_.capture = anim;
        lastCaptureCue_ = 0;
        feedback(a.timing == Timing::Perfect ? 40 : 20);
      } else {
        screen_.command = result;
      }
      dirty_ = true;
    } else if (screen_.page == Page::PracticeThrow && screen_.charging &&
               practiceCapture_.throwView().active && !input.held && !input.rightDown) {
      screen_.charging = false;
      const auto outcome = practiceCapture_.pressThrow(input.at);
      if (outcome.valid) startPracticeCapture(outcome);
      dirty_ = true;
    }
    return;
  }
  dirty_ = true;
  if (screen_.page == Page::Viewer) {
    if (input.back) goBack();
    else if (input.click && !input.left && !input.right &&
             viewer_screen::kEvolutionPreviewButton.contains(input.x, input.y))
      openEvolutionPreview(viewer_screen::species());
    return;
  }
  if (screen_.page == Page::Battle || screen_.page == Page::Practice) {
    const bool practice = screen_.page == Page::Practice;
    if (input.pause || (input.touch && input.y < 85)) {
      if (practice) practiceCapture_.requestPause(); else engine_.requestPause();
      return;
    }
    bool left = input.left || (input.touch && input.x < 233);
    bool right = input.right || (input.touch && input.x >= 233);
    if (left || right) {
      Action action = left && right ? Action::Both : left ? Action::Dodge : Action::Attack;
      if (practice) practiceCapture_.press(action, input.at); else engine_.press(action, input.at);
    }
    return;
  }
  if (screen_.page == Page::Throw) { handleThrow(input); return; }
  if (screen_.page == Page::PracticeThrow) { handlePracticeThrow(input); return; }
  if (input.back && touchBackEnabled(screen_.page)) { goBack(); return; }
  if (!input.left && !input.right && !input.click && !input.swipe) return;
  bool confirm = input.left && input.right;
  if (screen_.page == Page::Clock && screen_.editing) {
    if (confirm || (input.click && input.y >= 365)) screen_.editing = false;
    else if (input.left || input.right || input.click)
      editClock(input.left || (input.click && input.x < 233) ? -1 : 1);
    return;
  }
  const unsigned count = menuCount(screen_, engine_);
  if (!count) return;
  if (screen_.selected >= count) screen_.selected = 0;
  const bool record = screen_.page == Page::FirstCaptures || screen_.page == Page::Recent ||
                      screen_.page == Page::Days || screen_.page == Page::Achievements;
  if (record && input.swipe) {
    screen_.selected = (screen_.selected + count + input.swipe) % count;
    return;
  }
  if (record && input.click) {
    if (input.y >= 340) { navigate(Page::Records); return; }
    if (input.y >= 80)
      screen_.selected = (screen_.selected + (input.x < 233 ? count - 1 : 1)) % count;
    return;
  }
  if (input.swipe) {
    if (count > menu_layout::kPageSize)
      screen_.selected = input.swipe > 0 ? menu_layout::nextPage(screen_.selected, count)
                                         : menu_layout::previousPage(screen_.selected, count);
    return;
  }
  if (input.click) {
    const int top = screen_.page == Page::Result
                        ? (engine_.view().result == Result::Captured ? 372 : 355)
                        : screen_.page == Page::Menu ? menu_layout::kMainTop : menu_layout::kTop;
    const int row = menu_layout::rowAt(input.x, input.y, top);
    if (row >= 0) {
      const unsigned selected = menu_layout::first(screen_.selected) + unsigned(row);
      if (selected < count) { screen_.selected = selected; confirm = true; }
    } else if (count > menu_layout::kPageSize) {
      if (menu_layout::kPreviousPage.contains(input.x, input.y))
        screen_.selected = menu_layout::previousPage(screen_.selected, count);
      else if (menu_layout::kNextPage.contains(input.x, input.y))
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
      else if (item == 4) navigate(Page::Resources);
      else if (item == 5) beginPractice(at);
      else if (item == 6) sleepDisplay();
      else navigate(Page::Home);
      break;
    case Page::Resources:
      if (item == 0) navigate(Page::Records);
      else if (item == 1) openClock();
      else goBack();
      break;
    case Page::Routes:
      if (item < 3) { engine_.selectRoute(Route(item)); navigate(Page::Home); }
      else goBack();
      break;
    case Page::Partners: {
      unsigned row = 0;
      for (unsigned id = 1; id <= 151; ++id) if (owns(v, id)) {
        if (row++ == item) { openPartner(id); return; }
      }
      goBack(); break;
    }
    case Page::PartnerDetail: case Page::EvolutionConfirm: case Page::ReleaseConfirm:
      activateProgression(item, at); break;
    case Page::EvolutionPreviewTargets: chooseEvolutionPreview(item); break;
    case Page::Encounter:
      if (item == 0) { engine_.startBattle(at); navigate(Page::Battle); }
      else if (item == 1) beginPractice(at);
      else if (item == 2) navigate(Page::Partners);
      else navigate(Page::Home);
      break;
    case Page::Pause:
      if (screen_.practice) {
        if (item == 0) { practiceCapture_.resume(at); navigate(Page::Practice); }
        else endPractice();
      } else if (item == 0) { engine_.resume(at); navigate(Page::Battle); }
      else engine_.retreat();
      break;
    case Page::PracticeResult:
      if (item == 0) beginPractice(at); else endPractice();
      break;
    case Page::Result:
      engine_.acknowledgeResult(v.encounter.id, platform_.randomSeed()); navigate(Page::Home); break;
    case Page::Records: {
      if (item == 4) { goBack(); break; }
      const Page pages[] = {Page::FirstCaptures, Page::Recent, Page::Days, Page::Achievements};
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
