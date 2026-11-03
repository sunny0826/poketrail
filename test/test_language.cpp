#include "support/progression/harness.hpp"
#include "calendar.hpp"
using pokedex::i18n::Language;

static void normalSelection() {
  auto initial = fixture(1, 100); initial->progress[0].walked = 100;
  initial->route = Route::Sinnoh;
  MemoryStore store; store.disk = encoded(*initial); Engine engine(store);
  CHECK(engine.selectLanguage(Language::English) == CommandResult::NotReady);
  CHECK(engine.begin() == CommandResult::Applied);
  Save expected = engine.view(); ++expected.sequence; expected.language = Language::English;
  CHECK(engine.selectLanguage(Language::English) == CommandResult::Applied);
  CHECK(encoded(engine.view()) == encoded(expected) && store.disk == encoded(expected));
  const unsigned commits = store.commits;
  CHECK(engine.selectLanguage(Language::English) == CommandResult::Duplicate);
  CHECK(engine.selectLanguage(Language(255)) == CommandResult::InvalidInput);
  CHECK(store.commits == commits && encoded(engine.view()) == encoded(expected));
  Engine reboot(store); CHECK(reboot.begin() == CommandResult::Applied);
  CHECK(reboot.language() == Language::English);
  expected = reboot.view(); ++expected.sequence; expected.language = Language::Chinese;
  CHECK(reboot.selectLanguage(Language::Chinese) == CommandResult::Applied);
  CHECK(encoded(reboot.view()) == encoded(expected));
}
static void frozenPending() {
  for (bool after : {false, true}) {
    MemoryStore store; Engine engine(store); CHECK(engine.begin() == CommandResult::Applied);
    const auto old = encoded(engine.view());
    store.after = after; store.before = !after;
    CHECK(engine.selectLanguage(Language::English) == CommandResult::StorageError);
    CHECK(engine.savePending() && engine.language() == Language::Chinese && encoded(engine.view()) == old);
    const auto frozen = store.attempt; const unsigned commits = store.commits;
    CHECK(engine.selectLanguage(Language::Chinese) == CommandResult::SavePending);
    CHECK(engine.selectLanguage(Language::English) == CommandResult::SavePending);
    CHECK(store.commits == commits && store.attempt == frozen);
    MemoryStore restarted; restarted.disk = store.disk; Engine reboot(restarted);
    CHECK(reboot.begin() == CommandResult::Applied);
    CHECK(reboot.language() == (after ? Language::English : Language::Chinese));
    store.before = store.after = false;
    CHECK(engine.retrySave() == CommandResult::Applied);
    CHECK(engine.language() == Language::English && store.disk == frozen);
    CHECK(engine.retrySave() == CommandResult::Duplicate);
  }
  MemoryStore store; Engine engine(store); CHECK(engine.begin() == CommandResult::Applied);
  store.before = true;
  CHECK(engine.chooseStarter(4) == CommandResult::StorageError);
  const auto frozen = store.attempt;
  CHECK(engine.selectLanguage(Language::English) == CommandResult::SavePending && store.attempt == frozen);
  store.before = false; CHECK(engine.retrySave() == CommandResult::Applied);
  CHECK(engine.view().starter == 4 && engine.language() == Language::Chinese);
}
static void bootFailureLanguage() {
  for (bool after : {false, true}) {
    auto save = fixture(1); save->language = Language::English;
    MemoryStore store; store.disk = encoded(*save); store.before = !after; store.after = after;
    Engine engine(store); CHECK(engine.begin() == CommandResult::StorageError);
    CHECK(!engine.ready() && engine.savePending() && engine.language() == Language::English);
    CHECK(engine.selectLanguage(Language::Chinese) == CommandResult::SavePending);
    const auto frozen = store.attempt;
    store.before = store.after = false; CHECK(engine.retrySave() == CommandResult::Applied);
    CHECK(engine.ready() && engine.language() == Language::English && store.disk == frozen);
  }
}
struct QueueFailureStore : MemoryStore {
  unsigned failAt = 0;
  CommitStatus commit(const Save& save) override {
    before = commits + 1 == failAt;
    return MemoryStore::commit(save);
  }
};
static void committedLanguageThenStepFailure() {
  auto save = fixture(1, 999); save->progress[0].walked = 999;
  QueueFailureStore store; store.disk = encoded(*save); Engine engine(store);
  CHECK(engine.begin() == CommandResult::Applied);
  store.failAt = store.commits + 1;
  CHECK(engine.selectLanguage(Language::English) == CommandResult::StorageError);
  StepEvent step; step.batchId = 1; step.partner = 1; step.clock = clockFor(engine);
  CHECK(engine.observeSteps(&step, 1, 123) == CommandResult::SavePending);
  store.failAt = store.commits + 2;
  CHECK(engine.retrySave() == CommandResult::StorageError);
  CHECK(engine.savePending() && engine.language() == Language::English && engine.view().totalSteps == 999);
  const auto frozen = store.attempt;
  CHECK(engine.selectLanguage(Language::Chinese) == CommandResult::SavePending && store.attempt == frozen);
  store.failAt = 0; CHECK(engine.retrySave() == CommandResult::Applied);
  CHECK(engine.language() == Language::English && engine.view().totalSteps == 1000 &&
        engine.view().progress[0].walked == 1000 && store.disk == frozen);
}
static void activeOperations() {
  auto save = fixture(1, 300); save->progress[0].walked = 300;
  MemoryStore store; store.disk = encoded(*save); Engine engine(store);
  CHECK(engine.begin() == CommandResult::Applied);
  CHECK(steps(engine, 1, 1) == CommandResult::Applied && engine.view().phase == Phase::EncounterReady);
  CHECK(engine.startBattle(0) == CommandResult::Applied);
  const auto before = encoded(engine.view());
  CHECK(engine.selectLanguage(Language::English) == CommandResult::Deferred && encoded(engine.view()) == before);
  while (engine.view().phase == Phase::Battle) {
    const auto beat = engine.battleView();
    CHECK(engine.press(beat.expected, beat.targetMs) == CommandResult::Applied);
    if (engine.view().phase == Phase::Battle) CHECK(engine.tick(beat.targetMs + 181, clockFor(engine)) == CommandResult::Applied);
  }
  CHECK(engine.view().phase == Phase::ThrowReady);
  AttemptKey key; key.encounterId = engine.view().encounter.id; key.attemptIndex = 1;
  CHECK(engine.beginThrow(key, 10000) == CommandResult::Applied);
  CHECK(engine.selectLanguage(Language::English) == CommandResult::Deferred);
  CHECK(engine.cancelThrow(key) == CommandResult::Applied);
  CHECK(engine.selectLanguage(Language::English) == CommandResult::Applied);
}
static void pendingClock() {
  MemoryStore store; Engine engine(store); CHECK(engine.begin() == CommandResult::Applied);
  ClockReading target = clockFor(engine); target.trusted = true;
  const pokedex::calendar::CivilTime civil = {2026, 11, 2, 12, 0, 0};
  CHECK(pokedex::calendar::toUtc(civil, 0, target.utcSeconds));
  CHECK(pokedex::calendar::dayKey(civil, target.dayKey));
  CHECK(engine.beginClockChange(1, ClockChangeMode::Normal, target) == CommandResult::Applied);
  const auto before = encoded(engine.view()); const auto commits = store.commits;
  CHECK(engine.selectLanguage(Language::English) == CommandResult::Deferred);
  CHECK(encoded(engine.view()) == before && store.commits == commits);
  CHECK(engine.finishClockChange(1, target) == CommandResult::Applied);
  CHECK(engine.selectLanguage(Language::English) == CommandResult::Applied);
}
int main() {
  normalSelection(); frozenPending(); bootFailureLanguage(); committedLanguageThenStepFailure(); activeOperations(); pendingClock();
  std::printf("language: %u checks passed\n", checks);
}
