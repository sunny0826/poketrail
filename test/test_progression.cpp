#include "support/progression/harness.hpp"

static void allEdges() {
  size_t count=0;const auto* rules=evolutionRules(count);CHECK(count>0);
  for(size_t i=0;i<count;++i)for(int delta=-1;delta<=1;++delta){
    const auto& rule=rules[i];auto s=fixture(rule.from,rule.steps+delta);s->progress[rule.from-1].walked=rule.steps+delta;
    MemoryStore store;store.disk=encoded(*s);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
    const auto req=request(e,rule.from,rule.to);const auto old=encoded(e.view());
    CHECK(e.evolve(req,clockFor(e))==(delta<0?CommandResult::InsufficientProgress:CommandResult::Applied));
    if(delta<0){CHECK(encoded(e.view())==old);continue;}
    CHECK(!owns(e.view(),rule.from)&&owns(e.view(),rule.to)&&registered(e.view(),rule.from)&&registered(e.view(),rule.to));
    CHECK(e.view().progress[rule.from-1].walked-e.view().progress[rule.from-1].spent==uint64_t(delta));
    CHECK(e.view().progress[rule.to-1].walked==0&&e.view().partner==rule.to&&e.view().captureCounts[rule.to-1]==0);
    CHECK(e.view().totalSteps==s->totalSteps && e.view().explorationCredit==s->explorationCredit);
  }
}
static void accountsAndFailure() {
  auto s=fixture(1,9000);s->progress[0].walked=8000;s->progress[1].walked=1000;setRegistered(*s,2);
  MemoryStore store;store.disk=encoded(*s);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  auto req=request(e,1,2);store.before=true;CHECK(e.evolve(req,clockFor(e))==CommandResult::StorageError);
  const auto frozen=store.attempt;CHECK(e.view().partner==1 && e.view().progress[0].spent==0);
  CHECK(e.evolve(req,clockFor(e))==CommandResult::SavePending);
  CHECK(e.retrySave()==CommandResult::StorageError&&store.attempt==frozen);
  store.before=false;CHECK(e.retrySave()==CommandResult::Applied&&store.disk==frozen);
  CHECK(e.view().progress[0].spent==7500&&e.view().progress[1].walked==1000);
  CHECK(steps(e,1,1)==CommandResult::Applied&&e.view().progress[0].walked==8001);
  CHECK(steps(e,1,2)==CommandResult::Applied&&e.view().progress[1].walked==1001);
  CHECK(steps(e,1,0)==CommandResult::Applied&&e.view().progress[1].walked==1001);
  CHECK(steps(e,1,151)==CommandResult::InvalidInput);
  CHECK(e.checkpoint()==CommandResult::Applied);
  Engine reboot(store);CHECK(reboot.begin()==CommandResult::Applied);
  CHECK(reboot.evolve(req,clockFor(reboot))==CommandResult::Duplicate);
  CHECK(reboot.view().progress[0].walked==8001);
  auto both=fixture(1,8000);both->progress[0].walked=8000;setOwned(*both,2);setRegistered(*both,2);
  MemoryStore conflict;conflict.disk=encoded(*both);Engine c(conflict);CHECK(c.begin()==CommandResult::Applied);
  const auto before=encoded(c.view());CHECK(c.evolve(request(c,1,2),clockFor(c))==CommandResult::TargetOwned);
  CHECK(encoded(c.view())==before);
}
static void stepThresholds() {
  auto s=fixture(1,7499);s->progress[0].walked=7499;
  MemoryStore threshold;threshold.disk=encoded(*s);Engine t(threshold);CHECK(t.begin()==CommandResult::Applied);
  threshold.before=true;CHECK(steps(t,1,1)==CommandResult::StorageError);
  CHECK(t.view().progress[0].walked==7499);threshold.before=false;CHECK(t.retrySave()==CommandResult::Applied);
  const auto frozen=t.view().encounter;CHECK(t.evolve(request(t,1,2),clockFor(t))==CommandResult::Applied);
  CHECK(t.view().encounter.partner==2&&t.view().encounter.id==frozen.id&&t.view().encounter.seed==frozen.seed);
}
static void evolutionGuards() {
  auto pending=fixture(1,8000);pending->progress[0].walked=8000;
  pending->clock.pending.active=true;pending->clock.pending.transactionId=1;
  pending->clock.pending.targetUtcSeconds=1792627200;pending->clock.pending.targetDayKey=20260000+1022;
  MemoryStore ps;ps.disk=encoded(*pending);Engine pe(ps);CHECK(pe.begin()==CommandResult::Applied);
  CHECK(pe.evolve(request(pe,1,2),clockFor(pe))==CommandResult::InvalidPhase);
  CHECK(pe.selectPartner(1)==CommandResult::InvalidPhase);
  MemoryStore bs;Engine be(bs);CHECK(be.begin()==CommandResult::Applied);CHECK(be.chooseStarter(1)==CommandResult::Applied);
  CHECK(steps(be,7500,1)==CommandResult::Applied);CHECK(be.startBattle(0)==CommandResult::Applied);
  CHECK(be.requestPause()==CommandResult::Applied);CHECK(be.tick(be.battleView().targetMs+181,clockFor(be))==CommandResult::Applied);
  CHECK(be.evolve(request(be,1,2),clockFor(be))==CommandResult::InvalidPhase);
  CHECK(be.selectPartner(1)==CommandResult::InvalidPhase);
}
static void limitsAndValidation() {
  auto s=fixture(1,8000);s->progress[0].walked=8000;s->rosterRevision=UINT64_MAX;setOwned(*s,4);setRegistered(*s,4);
  MemoryStore store;store.disk=encoded(*s);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  const auto before=encoded(e.view());CHECK(e.evolve(request(e,1,2),clockFor(e))==CommandResult::InvalidInput);
  CHECK(e.selectPartner(4)==CommandResult::InvalidInput&&encoded(e.view())==before);
  auto max=fixture(1,UINT64_MAX);max->progress[0].walked=UINT64_MAX;
  MemoryStore ms;ms.disk=encoded(*max);Engine m(ms);CHECK(m.begin()==CommandResult::Applied);
  CHECK(steps(m,1,1)==CommandResult::Applied&&m.view().totalSteps==UINT64_MAX&&m.view().progress[0].walked==UINT64_MAX&&m.stepsIncomplete());
  auto bad=fixture(1,1000);bad->progress[0].walked=1001;CHECK(!validate(*bad));
  bad->progress[0].walked=1000;bad->progress[0].spent=1001;CHECK(!validate(*bad));
  bad->progress[0].spent=0;bad->ballsSpent=2;CHECK(!validate(*bad));
  bad->ballsSpent=0;bad->rewardBaseSteps=1001;CHECK(!validate(*bad));
}
int main(){allEdges();accountsAndFailure();stepThresholds();evolutionGuards();limitsAndValidation();std::printf("progression: %u checks passed\n",checks);}
