#pragma once

void scenarioSeeds() {
  ModeHost host;
  check(host.enterTest(TestSeed::Blank), "scenario host starts a blank test session");
  check(TestConsole::seed(host.test, host, TestScenario::AccountBalls),
        "account/ball scenario seeds the test store");
  pokedex::adventure::Save account;
  check(host.test.load(account).status == pokedex::adventure::LoadStatus::Loaded && validate(account),
        "account/ball scenario decodes as a valid save");
  check(account.totalSteps == 2000 && account.ballsSpent == 1 &&
        pokedex::adventure::availableBalls(account) == 1,
        "account/ball scenario exposes one remaining ball");

  check(TestConsole::seed(host.test, host, TestScenario::NearEncounter),
        "near-encounter scenario seeds the test store");
  pokedex::adventure::Save near;
  check(host.test.load(near).status == pokedex::adventure::LoadStatus::Loaded && validate(near),
        "near-encounter scenario decodes as a valid save");
  check(near.totalSteps == 299 && near.encounterTarget == 300 && near.phase == pokedex::adventure::Phase::Walking,
        "near-encounter scenario sits one step below its target");

  pokedex::adventure::Save current;
  check(TestConsole::seed(host.test, host, TestScenario::CurrentThrow),
        "current-rule throw scenario seeds the test store");
  check(host.test.load(current).status == pokedex::adventure::LoadStatus::Loaded && validate(current),
        "current-rule throw scenario decodes as a valid save");
  check(current.encounter.ruleVersion == pokedex::adventure::kRuleVersion &&
        current.encounter.usedBalls == 1 && current.encounter.attempts[0].used &&
        current.phase == pokedex::adventure::Phase::ThrowReady,
        "current-rule scenario waits for the second throw under rule 3");

  check(TestConsole::seed(host.test, host, TestScenario::LegacyEncounter),
        "legacy scenario seeds the test store");
  pokedex::adventure::Save legacy;
  check(host.test.load(legacy).status == pokedex::adventure::LoadStatus::Loaded && validate(legacy),
        "legacy scenario decodes as a valid save");
  check(legacy.legacyChargedCount == 1 && legacy.explorationSpent == 500 &&
        legacy.encounter.ruleVersion == 1 && legacy.encounter.usedBalls == 1 &&
        legacy.ballsSpent == 1 && pokedex::adventure::availableBalls(legacy) == 1 &&
        legacy.phase == pokedex::adventure::Phase::ThrowReady,
        "legacy scenario keeps the old-rule in-flight encounter and ball account");

  host.setFault(TestFault::CommitBefore);
  pokedex::adventure::Save before;
  check(host.test.load(before).status == pokedex::adventure::LoadStatus::Loaded,
        "scenario save loads before fault checks");
  before.sequence++;
  check(host.test.commit(before) == pokedex::adventure::CommitStatus::IoError,
        "write-before fault rejects the commit");
  host.setFault(TestFault::CommitAfter);
  pokedex::adventure::Save after;
  check(host.test.load(after).status == pokedex::adventure::LoadStatus::Loaded,
        "scenario save loads after write-before check");
  after.sequence++;
  check(host.test.commit(after) == pokedex::adventure::CommitStatus::IoError,
        "write-after fault reports failure after the bytes are written");
  host.setFault(TestFault::Load);
  check(host.test.load(before).status == pokedex::adventure::LoadStatus::IoError,
        "load fault rejects the test read");
  host.setFault(TestFault::None);
}
