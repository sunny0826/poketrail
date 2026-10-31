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
void Runtime::openHomeEvolution() {
  const uint16_t partner = engine_.view().partner;
  const EvolutionView view = engine_.evolutionView(partner);
  unsigned ready = view.count;
  for (unsigned i = 0; i < view.count; ++i)
    if (view.options[i].status == CommandResult::Applied) { ready = i; break; }
  if (ready == view.count) return;
  if (view.count > 1) {
    openPartner(partner);
    screen_.selected = ready + 1;
    return;
  }
  screen_.species = partner; screen_.evolved = false;
  screen_.evolution.from = view.from;
  screen_.evolution.to = view.options[ready].target;
  screen_.evolution.rosterRevision = view.revision;
  evolutionFromHome_ = true;
  navigate(Page::EvolutionConfirm);
}
void Runtime::leaveEvolutionConfirm() {
  if (evolutionFromHome_) navigate(Page::Home);
  else openPartner(screen_.species);
}
void Runtime::startEvolution() {
  const auto& request = engine_.view().lastEvolution.request;
  screen_.evolutionAnim.from = request.from;
  screen_.evolutionAnim.to = request.to;
  screen_.evolutionAnim.startMs = platform_.now();
  screen_.evolved = false;
  navigate(Page::Evolution);
}
bool Runtime::hasEvolutionPreview(uint16_t species, void* context) {
  return static_cast<Runtime*>(context)->engine_.evolutionView(species).count != 0;
}
void Runtime::openEvolutionPreview(uint16_t species) {
  const auto view = engine_.evolutionView(species);
  if (!view.count) return;
  screen_.evolutionAnim.from = species;
  if (view.count == 1) chooseEvolutionPreview(0);
  else navigate(Page::EvolutionPreviewTargets);
}
void Runtime::chooseEvolutionPreview(unsigned item) {
  const auto view = engine_.evolutionView(screen_.evolutionAnim.from);
  if (item >= view.count) { navigate(Page::Viewer); return; }
  screen_.evolutionAnim.to = view.options[item].target;
  screen_.evolutionAnim.startMs = platform_.now();
  navigate(Page::EvolutionPreview);
}
void Runtime::tickEvolution(uint32_t now, bool inputHeld) {
  if (screen_.page != Page::Evolution && screen_.page != Page::EvolutionPreview) return;
  if (evolutionFrameAt(screen_.evolutionAnim, now).stage == EvolutionStage::Done) {
    if (screen_.page == Page::EvolutionPreview) navigate(Page::Viewer);
    else {
      openPartner(screen_.evolutionAnim.to);
      screen_.evolved = true;
    }
    releaseGate_ = inputHeld;
  }
}
void Runtime::resumeSavedProgression() {
  const PendingRoster pending = pendingRoster_;
  pendingRoster_ = PendingRoster::None;
  bool evolved = false;
  if (pending == PendingRoster::Evolve) {
    const auto& receipt = engine_.view().lastEvolution.request;
    evolved = receipt.from == screen_.evolution.from && receipt.to == screen_.evolution.to &&
              receipt.rosterRevision == screen_.evolution.rosterRevision;
  }
  if (evolved) startEvolution();
  else openPartner(screen_.species);
}
void Runtime::activateProgression(unsigned item, uint32_t at) {
  if (screen_.page == Page::EvolutionConfirm) {
    if (item == 1) { leaveEvolutionConfirm(); return; }
    const CommandResult result = engine_.evolve(screen_.evolution, readingAt(at));
    if (result == CommandResult::Applied) {
      startEvolution();
    } else if (result == CommandResult::Duplicate) {
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
  evolutionFromHome_ = false;
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
void Runtime::handleThrow(const Input& input) {
  if (screen_.capture.active) {
    if (input.any()) { captureSkip(screen_.capture, input.at); waitForRelease(); }
    return;
  }
  const auto& encounter = engine_.view().encounter;
  const bool legacyNoBalls = encounter.ruleVersion < 3 && !availableBalls(engine_.view());
  if (legacyNoBalls) {
    // A legacy encounter with an empty ball stock cannot be thrown and the
    // menu is gone, so the page offers one explicit way out.
    if (input.click && menu_layout::rowAt(input.x, input.y) == 0) {
      screen_.command = engine_.giveUpCapture(encounter.id);
      waitForRelease();
      return;
    }
    // The side keys confirm the same single button.
    if (input.leftDown && input.rightDown) {
      screen_.command = engine_.giveUpCapture(encounter.id);
      waitForRelease();
      return;
    }
    if (input.click || input.swipe) return;
  }
  // Exclude the top touch region from starting a throw.
  const bool throwTouch = input.touch && input.y >= 80;
  if (engine_.throwView().active) {
    if ((input.right || throwTouch) && !screen_.charging) {
      screen_.charging = true;
      screen_.chargeStartMs = input.at;
      feedback(20);
    }
    return;
  }
  // Starting charge opens the window without spending an attempt.
  if ((input.right || throwTouch) && !screen_.charging) {
    screen_.command = engine_.beginThrow(AttemptKey(encounter.id, encounter.usedBalls + 1), input.at);
    if (screen_.command == CommandResult::Applied) {
      screen_.charging = true;
      screen_.chargeStartMs = input.at;
      feedback(20);
    }
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
  // The top edge ends the practice session, so a throw never starts there.
  const bool throwTouch = input.touch && input.y >= 80;
  if (practiceCapture_.throwView().active) {
    if ((input.right || throwTouch) && !screen_.charging) {
      screen_.charging = true;
      screen_.chargeStartMs = input.at;
      feedback(20);
    }
    return;
  }
  // Aiming state: the first hold starts the practice throw window. The session
  // still exits by touching the top edge, so practice never traps the player.
  if ((input.right || throwTouch) && !screen_.charging) {
    if (practiceCapture_.beginThrow(input.at)) {
      screen_.charging = true;
      screen_.chargeStartMs = input.at;
      feedback(20);
    }
  }
}
} }
