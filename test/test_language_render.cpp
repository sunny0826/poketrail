#include "support/progression_ui.hpp"
#include "support/rendered_text.hpp"
#include "support/language_catalogue.hpp"
using namespace progression_ui;
using pokedex::i18n::Language;
namespace {
void text(Runtime& runtime, const char* expected, uint16_t color = theme::kText) {
  frame(runtime, 60);
  check(rendered_text::contains(expected, color), expected);
}
void switchFromMenu(Runtime& runtime, Language language) {
  openMenu(runtime); select(runtime, 7);
  check(runtime.screen().page == Page::Language, "menu opens language page");
  select(runtime, unsigned(language)); frame(runtime);
}
void ready(MemoryStore& store, Language language = Language::Chinese, unsigned steps = 0) {
  Engine engine(store); check(engine.begin() == CommandResult::Applied, "fixture engine starts");
  check(engine.chooseStarter(1) == CommandResult::Applied, "fixture has a starter");
  if (steps) step(engine, 1, steps);
  const auto changed = engine.selectLanguage(language);
  check(changed == CommandResult::Applied || changed == CommandResult::Duplicate, "fixture language selected");
  engine.checkpoint();
}
void normal() {
  MemoryStore store; HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::DateWelcome, "blank Runtime shows welcome");
  select(runtime, 3); select(runtime, 1);
  check(runtime.language() == Language::English && runtime.engine().view().language == Language::English,
        "welcome selects and persists English");
  text(runtime, "Language"); screenshot(runtime, "language-selected");
  const auto writes = store.commits; select(runtime, 1);
  check(store.commits == writes, "same English language does not write");
  select(runtime, 2); select(runtime, 1);
  text(runtime, "Choose your starter"); screenshot(runtime, "english-starter");
  select(runtime, 0); text(runtime, "Bulbasaur"); screenshot(runtime, "english-home");
  openMenu(runtime); select(runtime, 3);
  text(runtime, "Bulbasaur"); text(runtime, "Grass / Poison", theme::kBlue);
  screenshot(runtime, "english-viewer"); tap(runtime, 233, 30);
  switchFromMenu(runtime, Language::Chinese);
  select(runtime, 2); select(runtime, 8); text(runtime, "妙蛙种子");
  Runtime reboot(store, platform); start(reboot);
  check(reboot.language() == Language::Chinese, "Runtime rebuild restores final language");
}
class UnavailableStore : public MemoryStore {
 public:
  bool unavailable = true;
  LoadResult load(Save& save) override {
    return unavailable ? LoadResult(LoadStatus::Corrupt) : MemoryStore::load(save);
  }
};
void temporaryRecovery() {
  UnavailableStore store; store.unavailable = false; ready(store); store.unavailable = true;
  const auto bytes = store.bytes; const unsigned commits = store.commits;
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  select(runtime, 2); select(runtime, 1);
  check(runtime.language() == Language::English && runtime.screen().languagePersistence == LanguagePersistence::SessionOnly,
        "failed load permits temporary English");
  screenshot(runtime, "language-session-only");
  select(runtime, 2); select(runtime, 1); text(runtime, "Collection unknown", theme::kMint);
  screenshot(runtime, "language-unavailable-viewer"); tap(runtime, 233, 30);
  check(store.bytes == bytes && store.commits == commits, "temporary selection never commits");
  store.unavailable = false; select(runtime, 0);
  check(runtime.engine().ready() && runtime.language() == Language::Chinese &&
        runtime.screen().languagePersistence == LanguagePersistence::Saved,
        "recovered save discards temporary language without saving it");
  text(runtime, "妙蛙种子"); screenshot(runtime, "language-load-recovered");
}
void bootFailure() {
  MemoryStore store; ready(store, Language::English); store.fail = true;
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  check(runtime.screen().page == Page::Error && !runtime.engine().ready() &&
        runtime.engine().savePending() && runtime.language() == Language::English,
        "boot write failure uses verified saved English");
  screenshot(runtime, "language-english-boot-failed");
  text(runtime, "Retry save / load", theme::kRed);
  select(runtime, 2); select(runtime, 0);
  check(runtime.language() == Language::English, "boot candidate cannot be replaced by Chinese");
  text(runtime, "Retry save first", theme::kGold);
  tap(runtime, 233, 30); store.fail = false; select(runtime, 0);
  check(runtime.engine().ready() && runtime.language() == Language::English, "boot retry publishes saved English");
  text(runtime, "Bulbasaur");
}
class StepFailureStore : public MemoryStore {
 public:
  bool blockSteps = false;
  CommitStatus commit(const Save& save) override {
    if (blockSteps && save.totalSteps > 999) { ++commits; return CommitStatus::IoError; }
    return MemoryStore::commit(save);
  }
};
void queuedStepFailure() {
  StepFailureStore store; ready(store, Language::Chinese, 999);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime);
  store.fail = true; switchFromMenu(runtime, Language::English);
  check(runtime.screen().page == Page::Error && runtime.language() == Language::Chinese, "language write failure stays Chinese");
  walk(runtime, platform, 5);
  check(runtime.engine().bufferedSteps() > 0, "detector queues real step events during pending language save");
  store.fail = false; store.blockSteps = true; select(runtime, 0);
  check(runtime.language() == Language::English && runtime.engine().savePending() && runtime.screen().page == Page::Error,
        "language success followed by step failure keeps English on the error page");
  text(runtime, "Retry save / load", theme::kRed); screenshot(runtime, "language-committed-step-failed");
  store.blockSteps = false; select(runtime, 0);
  check(!runtime.engine().savePending() && runtime.engine().view().totalSteps > 999 && runtime.language() == Language::English,
        "step retry finishes without reverting or reselecting language");
}
}
int main() {
  c152::begin("LANGUAGE TEST");
  normal(); temporaryRecovery(); bootFailure(); queuedStepFailure(); language_catalogue::verify();
  std::printf("%u language Runtime/M5GFX checks, 0 failures\n", checks);
}
