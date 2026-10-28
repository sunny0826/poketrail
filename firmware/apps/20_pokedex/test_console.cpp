#include "test_console.hpp"
#include "save_codec.hpp"
#include <cstddef>
#include <memory>
#include <new>

namespace pokedex { namespace adventure {
namespace {
void starter(Save& s) {
  s.starter = s.partner = 1;
  s.rosterRevision = 1;
  setRegistered(s, 1);
  setOwned(s, 1);
}
void buildScenario(Save& s, TestScenario scenario, uint64_t sequence) {
  s.~Save();
  new (&s) Save();
  s.schema = kSchemaVersion;
  s.sequence = sequence;
  s.bootGeneration = 1;
  s.encounterTarget = 300;
  if (scenario == TestScenario::AccountBalls) {
    starter(s);
    s.totalSteps = 2000;
    s.explorationCredit = 2000;
    s.ballsSpent = 1;
    s.progress[0].walked = 2000;
    return;
  }
  if (scenario == TestScenario::NearEncounter) {
    starter(s);
    s.totalSteps = 299;
    s.explorationCredit = 299;
    s.progress[0].walked = 299;
    return;
  }
  if (scenario == TestScenario::CurrentThrow) {
    starter(s);
    s.totalSteps = 800;
    s.explorationSpent = 300;
    s.explorationCredit = 500;
    s.progress[0].walked = 800;
    s.nextEncounterId = 2;
    s.phase = Phase::ThrowReady;
    s.result = Result::Won;
    Encounter& e = s.encounter;
    e.id = 1;
    e.species = 16;
    e.partner = s.partner;
    e.route = Route::Forest;
    e.seed = 0x2468ace0u;
    e.ruleVersion = kRuleVersion;
    e.usedBalls = 1;
    e.attempts[0].used = true;
    e.attempts[0].timing = Timing::Miss;
    e.attempts[0].probability = 0;
    e.attempts[0].roll = throwRoll(e, 1, kRuleVersion);
    e.attempts[0].ruleVersion = kRuleVersion;
    return;
  }
  starter(s);
  s.totalSteps = 2000;
  s.explorationSpent = 500;
  s.explorationCredit = 1500;
  s.legacyChargedCount = 1;
  s.ballsSpent = 1;
  s.progress[0].walked = 2000;
  s.nextEncounterId = 2;
  s.phase = Phase::ThrowReady;
  s.result = Result::Won;
  Encounter& e = s.encounter;
  e.id = 1;
  e.species = 16;
  e.partner = s.partner;
  e.route = Route::Forest;
  e.seed = 0x12345678u;
  e.ruleVersion = 1;
  e.usedBalls = 1;
  e.attempts[0].used = true;
  e.attempts[0].timing = Timing::Miss;
  e.attempts[0].probability = 0;
  e.attempts[0].roll = throwRoll(e, 1, 1);
  e.attempts[0].ruleVersion = 1;
}
}

bool TestConsole::seed(Store& store, TestModeHost& host, TestScenario scenario) {
  if (!host.testMode()) return false;
  host.setFault(TestFault::None);
  std::unique_ptr<Save> candidate(new (std::nothrow) Save());
  if (!candidate) return false;
  const LoadResult loaded = store.load(*candidate);
  uint64_t sequence = 0;
  if (loaded.status == LoadStatus::Loaded) sequence = candidate->sequence;
  else if (loaded.status != LoadStatus::Empty) {
    if (!host.enterTest(TestSeed::Blank)) return false;
  }
  if (sequence == UINT64_MAX) return false;
  buildScenario(*candidate, scenario, sequence + 1);
  if (!validate(*candidate)) return false;
  // Seed buffers live on the heap so official mode pays no 24 KB static cost
  // and the firmware task stack never holds an encoded save.
  std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[codec::maxEncodedSize]);
  if (!bytes) return false;
  size_t length = 0;
  if (!codec::encode(*candidate, bytes.get(), codec::maxEncodedSize, length)) return false;
  if (codec::decode(bytes.get(), length, *candidate) != codec::DecodeStatus::Ok ||
      !validate(*candidate)) return false;
  return store.commit(*candidate) == CommitStatus::Committed;
}

const char* testScenarioLabel(TestScenario scenario) {
  switch (scenario) {
    case TestScenario::AccountBalls: return "账户 · 2000步 / 1球";
    case TestScenario::NearEncounter: return "临近遭遇 · 299/300步";
    case TestScenario::CurrentThrow: return "当前投球 · 已用1球";
    case TestScenario::LegacyEncounter: return "旧规则 · rule1 在途";
  }
  return "";
}

const char* testFaultLabel(TestFault fault) {
  switch (fault) {
    case TestFault::None: return "无故障";
    case TestFault::CommitBefore: return "写入前失败";
    case TestFault::CommitAfter: return "写入后失败";
    case TestFault::Load: return "读取失败";
  }
  return "";
}

} }
