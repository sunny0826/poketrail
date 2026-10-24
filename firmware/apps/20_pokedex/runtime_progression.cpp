#include "runtime.hpp"
#include "menu_layout.hpp"
#include "battle_presentation.hpp"
namespace pokedex { namespace ui {
using namespace adventure;
const char* Runtime::collectionLabel(uint16_t species, void* context) {
  const Runtime& runtime = *static_cast<Runtime*>(context);
  if (!runtime.engine_.ready()) return "收藏状态未知";
  const Save& save = runtime.engine_.view();
  return owns(save, species) ? "当前拥有" : registered(save, species) ? "曾经获得" : "尚未获得";
}
void Runtime::openPartner(uint16_t species) {
  screen_.species = species; screen_.evolved = false;
  navigate(Page::PartnerDetail);
}
void Runtime::resumeSavedProgression() {
  const PendingRoster pending = pendingRoster_;
  pendingRoster_ = PendingRoster::None;
  bool evolved = false;
  if (pending == PendingRoster::Evolve) {
    const auto& receipt = engine_.view().lastEvolution.request;
    evolved = receipt.from == screen_.evolution.from && receipt.to == screen_.evolution.to &&
              receipt.rosterRevision == screen_.evolution.rosterRevision;
    if (evolved) screen_.species = receipt.to;
  }
  openPartner(screen_.species);
  screen_.evolved = evolved;
}
void Runtime::activateProgression(unsigned item, uint32_t at) {
  if (screen_.page == Page::ReleaseConfirm) {
    if (item == 1) navigate(Page::Throw);
    else {
      screen_.command = engine_.giveUpCapture(engine_.view().encounter.id);
      waitForRelease();
    }
    return;
  }
  if (screen_.page == Page::EvolutionConfirm) {
    if (item == 1) { openPartner(screen_.species); return; }
    const CommandResult result = engine_.evolve(screen_.evolution, readingAt(at));
    if (result == CommandResult::Applied || result == CommandResult::Duplicate) {
      openPartner(screen_.evolution.to); screen_.evolved = true;
    } else {
      screen_.command = result; waitForRelease();
      if (engine_.savePending()) pendingRoster_ = PendingRoster::Evolve;
    }
    return;
  }
  const EvolutionView view = engine_.evolutionView(screen_.species);
  if (item == view.count + 1) { navigate(Page::Partners); return; }
  if (item == 0) {
    const CommandResult result = engine_.selectPartner(screen_.species);
    screen_.command = result; screen_.evolved = false; waitForRelease();
    if (engine_.savePending()) pendingRoster_ = PendingRoster::Select;
    return;
  }
  if (item > view.count) return;
  const EvolutionOption& option = view.options[item - 1];
  if (option.status != CommandResult::Applied) {
    screen_.command = option.status; screen_.evolved = false; return;
  }
  screen_.evolution.from = view.from;
  screen_.evolution.to = option.target;
  screen_.evolution.rosterRevision = view.revision;
  navigate(Page::EvolutionConfirm);
}
void Runtime::tickCapture(uint32_t now) {
  if (screen_.page == Page::Throw && screen_.charging && !engine_.throwView().active)
    screen_.charging = false;
  if (screen_.page == Page::PracticeThrow && screen_.charging && !practiceCapture_.throwView().active)
    screen_.charging = false;
  if (!screen_.capture.active) { lastCaptureCue_ = 0; return; }
  const auto cue = uint8_t(captureCueAt(screen_.capture, now));
  if (cue != lastCaptureCue_) {
    lastCaptureCue_ = cue;
    switch (CaptureCue(cue)) {
      case CaptureCue::Land1: case CaptureCue::Land2: feedback(10); break;
      case CaptureCue::Wiggle1: case CaptureCue::Wiggle2: case CaptureCue::Wiggle3:
        feedback(15); break;
      case CaptureCue::BreakOut: feedback(25); break;
      case CaptureCue::Success1: case CaptureCue::Success2: case CaptureCue::Success3:
        feedback(30); break;
      default: break;
    }
    dirty_ = true;
  }
  if (captureFrameAt(screen_.capture, now).stage == CaptureStage::Done) {
    screen_.capture = CaptureAnim();
    dirty_ = true;
    if (screen_.page == Page::PracticeThrow && practiceCapture_.stage() == PracticeStage::ThrowReady)
      waitForRelease();
  }
}
void Runtime::handleThrow(const Input& input) {  if (screen_.capture.active) {
    if (input.any()) { captureSkip(screen_.capture, input.at); waitForRelease(); }
    return;
  }
  if (engine_.throwView().active) {
    if ((input.right || input.touch) && !screen_.charging) {
      screen_.charging = true;
      screen_.chargeStartMs = input.at;
      feedback(20);
    }
    return;
  }
  bool confirm = input.left && input.right;
  if (input.click) {
    const int row = menu_layout::rowAt(input.x, input.y);
    if (row >= 0 && row < 3) {
      screen_.selected = static_cast<unsigned>(row); confirm = true;
    }
  } else if (!confirm && input.left) {
    screen_.selected = (screen_.selected + 1) % 3;
    screen_.command = CommandResult::Applied;
  } else if (input.right) confirm = true;
  if (!confirm) return;
  if (screen_.selected == 1) {
    deferredThrow_ = engine_.view().encounter.id;
    navigate(Page::Home);
  } else if (screen_.selected == 2) navigate(Page::ReleaseConfirm);
  else {
    const auto& encounter = engine_.view().encounter;
    screen_.command = engine_.beginThrow(AttemptKey(encounter.id, encounter.usedBalls + 1), input.at);
    waitForRelease();
  }
}
void Runtime::beginPractice(uint32_t at) {
  screen_.practice = true;
  screen_.charging = false;
  screen_.capture = CaptureAnim();
  practiceCapture_.begin(at, platform_.randomSeed());
  screen_.practiceSpecies = practiceCapture_.species();
  navigate(Page::Practice);
}
void Runtime::endPractice() {
  const bool practicePage = screen_.page == Page::Practice || screen_.page == Page::PracticeThrow ||
                            screen_.page == Page::PracticeResult || screen_.page == Page::Pause;
  practiceCapture_.reset();
  screen_.practice = false;
  screen_.charging = false;
  screen_.capture = CaptureAnim();
  screen_.practiceSpecies = 0;
  screen_.practiceThrowing = false;
  screen_.practiceUsedBalls = 0;
  screen_.practiceResult = Result::None;
  if (practicePage) navigate(Page::Home);
}
void Runtime::syncPracticeView() {
  screen_.practiceSpecies = practiceCapture_.species();
  screen_.practiceThrowing = practiceCapture_.throwView().active;
  screen_.practiceTargetMs = practiceCapture_.throwView().targetMs;
  screen_.practiceUsedBalls = practiceCapture_.usedBalls();
  screen_.practiceResult = practiceCapture_.result();
}
void Runtime::syncPracticePage() {
  const PracticeStage stage = practiceCapture_.stage();
  if (stage == PracticeStage::Idle) {
    if (screen_.practice) {
      screen_.practice = false;
      if (screen_.page == Page::Practice || screen_.page == Page::PracticeThrow ||
          screen_.page == Page::PracticeResult || screen_.page == Page::Pause)
        navigate(Page::Home);
    }
    return;
  }
  if (stage == PracticeStage::Battle) {
    if (practiceCapture_.battleView().paused) {
      if (screen_.page != Page::Pause) navigate(Page::Pause);
    } else if (screen_.page != Page::Practice) navigate(Page::Practice);
    return;
  }
  if (screen_.page == Page::Practice && battleEffectActive(practiceCapture_.battleView(), platform_.now())) return;
  if (stage == PracticeStage::Result) {
    if (screen_.capture.active) return;
    if (screen_.page != Page::PracticeResult) {
      navigate(Page::PracticeResult);
      waitForRelease();
    }
    return;
  }
  if (screen_.page != Page::PracticeThrow) navigate(Page::PracticeThrow);
}
void Runtime::startPracticeCapture(const PracticeOutcome& outcome) {
  CaptureAnim anim;
  anim.active = true;
  anim.startMs = outcome.at;
  anim.timing = outcome.timing;
  anim.captured = outcome.captured;
  anim.wiggleBreak = outcome.captured ? 0 : uint8_t(1 + outcome.roll % 3);
  screen_.capture = anim;
  lastCaptureCue_ = 0;
  feedback(outcome.timing == Timing::Perfect ? 40 : 20);
  dirty_ = true;
}
void Runtime::handlePracticeThrow(const Input& input) {
  if (screen_.capture.active) {
    if (input.any()) { captureSkip(screen_.capture, input.at); waitForRelease(); }
    return;
  }
  if (practiceCapture_.throwView().active) {
    if ((input.right || input.touch) && !screen_.charging) {
      screen_.charging = true;
      screen_.chargeStartMs = input.at;
      feedback(20);
    }
    return;
  }
  bool confirm = input.left && input.right;
  if (input.click) {
    const int row = menu_layout::rowAt(input.x, input.y);
    if (row >= 0 && row < 3) {
      screen_.selected = static_cast<unsigned>(row); confirm = true;
    }
  } else if (!confirm && input.left) {
    screen_.selected = (screen_.selected + 1) % 3;
  } else if (input.right) confirm = true;
  if (!confirm) return;
  if (screen_.selected == 1) beginPractice(input.at);
  else if (screen_.selected == 2) endPractice();
  else if (practiceCapture_.beginThrow(input.at)) waitForRelease();
}
} }
