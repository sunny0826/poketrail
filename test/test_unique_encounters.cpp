#include "support/progression/harness.hpp"

static std::unique_ptr<Save> collection(uint16_t missing = 0, Route route = Route::Forest) {
  auto s = fixture(1);
  unsigned captures = 0;
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    if (!ordinaryEncounter(id) || id == missing) continue;
    setRegistered(*s, id); setOwned(*s, id);
    s->captureCounts[id - 1] = 1;
    Capture& c = s->firstCapture[id - 1]; c.species = id;
    c.at.bootGeneration = 1; c.at.totalSteps = ++captures * kLegacyEncounterSteps;
  }
  total(*s, (captures + 1) * kLegacyEncounterSteps);
  s->explorationCredit = kLegacyEncounterSteps; s->nextEncounterId = captures + 1; s->route = route;
  s->explorationSpent = s->totalSteps - s->explorationCredit;
  s->progress[0].walked = 8000;
  CHECK(validate(*s)); return s;
}
static void samplePools() {
  auto full = collection();
  for (unsigned r = 0; r < 3; ++r) {
    CHECK(remainingEncounters(*full, Route(r)) == 0);
    for (unsigned seed = 0; seed < 100; ++seed) CHECK(chooseEncounter(*full, Route(r), seed) == 0);
  }
  CHECK(remainingEncounters(*full, Route(3)) == 0);
  for (uint16_t id = 1; id <= kSpeciesCount; ++id) {
    if (!ordinaryEncounter(id)) continue;
    auto one = collection(id);
    for (unsigned r = 0; r < 3; ++r) {
      const bool available = catalogEntry(id)->routes & (1u << r);
      CHECK(remainingEncounters(*one, Route(r)) == unsigned(available));
      for (unsigned seed = 0; seed < 100; ++seed)
        CHECK(chooseEncounter(*one, Route(r), seed) == (available ? id : 0));
    }
  }
}
static void exhaustionAndSwitch() {
  MemoryStore store; auto all = collection(); store.disk = encoded(*all);
  Engine e(store); CHECK(e.begin() == CommandResult::Applied);
  const uint64_t id = e.view().nextEncounterId, credit = e.view().explorationCredit;
  const uint64_t balls = availableBalls(e.view());
  CHECK(steps(e, 1000, 1) == CommandResult::Applied);
  CHECK(e.view().phase == Phase::Walking && e.view().nextEncounterId == id && !e.view().encounter.id);
  CHECK(e.view().explorationCredit == credit + 1000 && availableBalls(e.view()) == balls + 1);
  CHECK(e.view().progress[0].walked == 9000);
  CHECK(e.selectRoute(Route::Coast) == CommandResult::Applied && e.view().nextEncounterId == id);
  CHECK(e.evolve(request(e, 1, 2), clockFor(e)) == CommandResult::Applied);
  CHECK(!owns(e.view(), 1) && e.view().captureCounts[0] == 1);
  CHECK(chooseEncounter(e.view(), Route::Forest, 7) == 0);
  CHECK(e.checkpoint() == CommandResult::Duplicate);
  Engine restart(store); CHECK(restart.begin() == CommandResult::Applied);
  CHECK(!remainingEncounters(restart.view(), Route::Forest) && restart.view().explorationCredit == credit + 1000);

  auto coastal = collection(129, Route::Forest); MemoryStore coastStore; coastStore.disk = encoded(*coastal);
  Engine coast(coastStore); CHECK(coast.begin() == CommandResult::Applied);
  CHECK(remainingEncounters(coast.view(), Route::Forest) == 0);
  CHECK(coast.selectRoute(Route::Coast) == CommandResult::Applied);
  CHECK(coast.view().phase == Phase::EncounterReady && coast.view().encounter.species == 129);
  CHECK(coast.view().explorationCredit == kLegacyEncounterSteps - coastal->encounterTarget &&
        coast.view().nextEncounterId == coastal->nextEncounterId + 1);
}
static void failedEncountersRemain() {
  auto s = collection(1); MemoryStore store; store.disk = encoded(*s);
  Engine e(store); CHECK(e.begin() == CommandResult::Applied);
  CHECK(steps(e, 1, 1) == CommandResult::Applied && e.view().encounter.species == 1);
  CHECK(e.retreat() == CommandResult::Applied);
  CHECK(steps(e, 5000, 1) == CommandResult::Applied);
  CHECK(e.acknowledgeResult(e.view().encounter.id, 7) == CommandResult::Applied);
  CHECK(e.view().encounter.species == 1 && e.view().captureCounts[0] == 0);
  CHECK(e.startBattle(0) == CommandResult::Applied);
  CHECK(e.tick(30000, clockFor(e)) == CommandResult::Applied && e.view().result == Result::Lost);
  CHECK(steps(e, 5000, 1) == CommandResult::Applied);
  CHECK(e.acknowledgeResult(e.view().encounter.id, 7) == CommandResult::Applied);
  CHECK(e.view().encounter.species == 1); win(e);
  for (uint8_t attempt = 1; attempt <= 2; ++attempt) {
    CHECK(e.beginThrow(AttemptKey(e.view().encounter.id, attempt), 0) == CommandResult::Applied);
    CHECK(e.tick(e.throwView().targetMs + 181, clockFor(e)) == CommandResult::Applied);
  }
  CHECK(e.view().result == Result::Escaped && e.view().captureCounts[0] == 0);
  CHECK(steps(e, 5000, 1) == CommandResult::Applied);
  CHECK(e.acknowledgeResult(e.view().encounter.id, 7) == CommandResult::Applied); win(e);
  CHECK(e.giveUpCapture(e.view().encounter.id) == CommandResult::Applied);
  CHECK(e.view().captureCounts[0] == 0 && remainingEncounters(e.view(), Route::Forest) == 1);
}
static void captureRetry() {
  auto s = collection(1); MemoryStore store; store.disk = encoded(*s);
  Engine e(store); CHECK(e.begin() == CommandResult::Applied);
  uint32_t seed = 0; Encounter encounter; encounter.id = e.view().nextEncounterId;
  for (;; ++seed) { encounter.seed = seed; if (throwRoll(encounter, 1, kRuleVersion) < 98) break; }
  StepEvent step; step.batchId = 1; step.partner = 1; step.clock = clockFor(e);
  CHECK(e.observeSteps(&step, 1, seed) == CommandResult::Applied); win(e);
  const AttemptKey key(e.view().encounter.id, 1);
  CHECK(e.beginThrow(key, 0) == CommandResult::Applied); store.after = true;
  CHECK(e.pressThrow(key, e.throwView().targetMs, clockFor(e)) == CommandResult::StorageError);
  CHECK(e.view().captureCounts[0] == 0);
  const auto bytes = store.attempt;
  CHECK(e.retrySave() == CommandResult::StorageError && bytes == store.attempt);
  store.after = false; CHECK(e.retrySave() == CommandResult::Applied && store.disk == bytes);
  CHECK(e.view().captureCounts[0] == 1 && !remainingEncounters(e.view(), Route::Forest));
  CHECK(e.pressThrow(key, 0, clockFor(e)) == CommandResult::Duplicate);
  Engine restart(store); CHECK(restart.begin() == CommandResult::Applied);
  CHECK(restart.view().captureCounts[0] == 1 && restart.view().result == Result::Captured);
  CHECK(restart.acknowledgeResult(key.encounterId, 7) == CommandResult::Applied);
  CHECK(steps(restart, 5000, 1) == CommandResult::Applied);
  CHECK(restart.view().phase == Phase::Walking && restart.view().captureCounts[0] == 1);
}
static void kantoOnly(Save& s) {
  for (uint16_t id = 152; id <= kSpeciesCount; ++id) {
    const uint8_t mask = uint8_t(~(1u << ((id - 1) % 8)));
    s.owned[(id - 1) / 8] &= mask; s.registered[(id - 1) / 8] &= mask;
    s.captureCounts[id - 1] = 0; s.firstCapture[id - 1] = Capture(); s.progress[id - 1] = SpeciesProgress();
  }
}
static void pendingOldDuplicate() {
  for (unsigned schema : {1u, 2u}) for (Phase phase : {Phase::EncounterReady, Phase::Battle, Phase::ThrowReady}) {
    auto s = collection();
    if (schema < 3) kantoOnly(*s);
    s->captureCounts[0] = 3;
    s->nextEncounterId += 2; total(*s, s->totalSteps + 2 * kLegacyEncounterSteps);
    s->explorationCredit = kLegacyEncounterSteps;
    s->phase = phase; s->result = phase == Phase::ThrowReady ? Result::Won : Result::None;
    s->encounter.id = s->nextEncounterId++; s->encounter.species = s->encounter.partner = 1;
    s->encounter.ruleVersion = schema == 1 ? 1 : 2;
    for (auto& attempt : s->encounter.attempts) attempt.ruleVersion = s->encounter.ruleVersion;
    s->explorationCredit -= kLegacyEncounterSteps;
    if (phase == Phase::ThrowReady) {
      s->ballsSpent = s->encounter.usedBalls = 1;
      auto& attempt = s->encounter.attempts[0]; attempt.used = true;
      attempt.roll = throwRoll(s->encounter, 1, s->encounter.ruleVersion);
    }
    if (schema == 1) {
      s->schema = 1;
      s->explorationCredit = s->totalSteps - (s->nextEncounterId - 1) * 500;
    } else {
      s->schema = schema;
      total(*s, s->nextEncounterId * kLegacyEncounterSteps);
      s->explorationCredit = kLegacyEncounterSteps;
    }
    s->explorationSpent = s->schema == 1 ? 0 : s->totalSteps - s->explorationCredit;
    CHECK(validate(*s));
    MemoryStore store; store.disk = encoded(*s); store.before = true;
    Engine e(store); CHECK(e.begin() == CommandResult::StorageError && !e.ready());
    const auto frozen = store.attempt;
    CHECK(e.retrySave() == CommandResult::StorageError && store.attempt == frozen);
    store.before = false; CHECK(e.retrySave() == CommandResult::Applied);
    CHECK(e.view().phase == Phase::Resolved && e.view().result == Result::Released);
    CHECK(e.view().captureCounts == s->captureCounts);
    CHECK(e.view().ballsSpent == (schema == 1 ? 0 : s->ballsSpent));
    CHECK(e.view().encounter.usedBalls == s->encounter.usedBalls);
    CHECK(e.view().encounter.legacyFreeAttempts == (schema == 1 ? s->encounter.usedBalls : 0));
    CHECK(e.view().explorationCredit == s->explorationCredit && e.view().nextEncounterId == s->nextEncounterId);
    CHECK(e.beginThrow(AttemptKey(s->encounter.id, s->encounter.usedBalls + 1), 0) == CommandResult::InvalidPhase);
    Engine restart(store); CHECK(restart.begin() == CommandResult::Applied);
    CHECK(restart.view().result == Result::Released && restart.view().captureCounts[0] == 3);
  }
}
int main() {
  samplePools(); exhaustionAndSwitch(); failedEncountersRemain(); captureRetry(); pendingOldDuplicate();
  std::printf("unique encounters: %u checks passed\n", checks);
}
