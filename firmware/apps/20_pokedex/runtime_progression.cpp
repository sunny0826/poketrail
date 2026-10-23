#include "runtime.hpp"
#include "menu_layout.hpp"
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
void Runtime::handleThrow(const Input& input) {
  if (engine_.throwView().active) {
    if (input.right || input.touch) {
      engine_.pressThrow(engine_.throwView().key, input.at, readingAt(input.at));
      waitForRelease();
    }
    return;
  }
  bool confirm = input.left && input.right;
  if (input.touch) {
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
} }
