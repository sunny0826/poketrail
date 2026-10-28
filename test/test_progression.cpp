#include "support/progression/harness.hpp"
static_assert(kSchemaVersion==5 && kRuleVersion==3 && kEncounterMinSteps==200 && kEncounterMaxSteps==500,"v5 save, v3 encounter rules, random cadence");
static void allEdges() {
  size_t count=0;const auto* rules=evolutionRules(count);CHECK(count==122);
  for(size_t i=0;i<count;++i)for(int delta=-1;delta<=1;++delta){
    const auto& rule=rules[i];auto s=fixture(rule.from,rule.steps+delta);s->progress[rule.from-1].walked=rule.steps+delta;
    MemoryStore store;store.disk=encoded(*s);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
    const auto req=request(e,rule.from,rule.to);const auto old=encoded(e.view());const auto view=e.evolutionView(rule.from);
    CHECK(view.count>=1 && view.count<=kEvolutionOptions && view.available==uint64_t(rule.steps+delta));
    CHECK(e.evolve(req,clockFor(e))==(delta<0?CommandResult::InsufficientProgress:CommandResult::Applied));
    if(delta<0){CHECK(encoded(e.view())==old);continue;}
    CHECK(!owns(e.view(),rule.from)&&owns(e.view(),rule.to)&&registered(e.view(),rule.from)&&registered(e.view(),rule.to));
    CHECK(e.view().progress[rule.from-1].walked-e.view().progress[rule.from-1].spent==uint64_t(delta));
    CHECK(e.view().progress[rule.to-1].walked==0&&e.view().partner==rule.to&&e.view().captureCounts[rule.to-1]==0);
    CHECK(e.evolve(req,clockFor(e))==CommandResult::Duplicate);
    auto stale=req;stale.to=151;CHECK(e.evolve(stale,clockFor(e))==CommandResult::Stale);
    CHECK(e.view().totalSteps==s->totalSteps && e.view().explorationCredit==s->explorationCredit);
    CHECK(validate(e.view()));
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
  CHECK(c.evolutionView(2).options[0].status==CommandResult::InvalidInput);
  CHECK(c.selectPartner(2)==CommandResult::Applied);CHECK(c.view().rosterRevision==2);
  CHECK(c.evolve(request(c,1,2),clockFor(c))==CommandResult::InvalidInput);
  auto eevee=fixture(133,10000);eevee->progress[132].walked=10000;setOwned(*eevee,134);setRegistered(*eevee,134);
  MemoryStore es;es.disk=encoded(*eevee);Engine ev(es);CHECK(ev.begin()==CommandResult::Applied);
  const auto view=ev.evolutionView(133);CHECK(view.count==5);
  for(const auto& option:view.options)CHECK(option.status==(option.target==134?CommandResult::TargetOwned:CommandResult::Applied));
  CHECK(ev.evolutionView(134).count==0);
}
static void johtoRules() {
  struct Case{uint16_t from,to;uint64_t cost;};
  const Case cases[]={{152,153,7500},{153,154,8000},{155,156,6500},{156,157,11000},
                      {158,159,8500},{159,160,6000},{161,162,7000},{188,189,4500},
                      {204,205,15000},{172,25,10000},{42,169,10000},{44,182,10000},
                      {133,196,10000},{133,197,10000},{95,208,10000},{113,242,10000},
                      {238,124,14500},{246,247,14500},{247,248,12500}};
  for(const Case& c:cases){const auto* rule=evolutionRule(c.from,c.to);
    CHECK(rule&&rule->steps==c.cost);}
  for(uint16_t target:{106,107,237}){const auto* rule=evolutionRule(236,target);
    CHECK(rule&&rule->steps==10000);}
  auto tyrogue=fixture(236,10000);tyrogue->progress[235].walked=10000;
  MemoryStore ts;ts.disk=encoded(*tyrogue);Engine te(ts);CHECK(te.begin()==CommandResult::Applied);
  const auto three=te.evolutionView(236);CHECK(three.count==3);
  for(unsigned i=0;i<3;++i){const auto& option=three.options[i];
    CHECK((option.target==106||option.target==107||option.target==237)&&option.cost==10000&&
          option.status==CommandResult::Applied);}
  const uint16_t eeveeTargets[]={134,135,136,196,197};
  auto eevee=fixture(133,10000);eevee->progress[132].walked=10000;
  MemoryStore es;es.disk=encoded(*eevee);Engine ev(es);CHECK(ev.begin()==CommandResult::Applied);
  const auto five=ev.evolutionView(133);CHECK(five.count==5);
  for(unsigned i=0;i<5;++i)CHECK(five.options[i].target==eeveeTargets[i]&&five.options[i].cost==10000);
  CHECK(ev.evolve(request(ev,133,197),clockFor(ev))==CommandResult::Applied&&owns(ev.view(),197));
  CHECK(registered(ev.view(),133)&&!owns(ev.view(),133)&&ev.view().progress[132].spent==10000);
  CHECK(ev.evolutionView(196).count==0&&ev.evolutionView(197).count==0);
}
static void stepThresholds() {
  MemoryStore store;Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  CHECK(steps(e,999,0)==CommandResult::Applied&&availableBalls(e.view())==0);
  const auto commits=store.commits;CHECK(steps(e,1,0)==CommandResult::Applied&&store.commits==commits+1);
  CHECK(availableBalls(e.view())==1&&e.view().progress[0].walked==0);
  CHECK(e.chooseStarter(1)==CommandResult::Applied);
  CHECK(e.view().phase==Phase::EncounterReady&&e.view().encounter.id==1);
  const uint64_t spent0=e.view().explorationSpent;
  CHECK(spent0>=kEncounterMinSteps&&spent0<=kEncounterMaxSteps&&e.view().explorationCredit==1000-spent0);
  const auto first=e.view().encounter;CHECK(steps(e,1,1)==CommandResult::Applied&&e.view().explorationCredit==1001-spent0);
  CHECK(steps(e,3999,1)==CommandResult::Applied&&e.view().explorationCredit==5000-spent0&&availableBalls(e.view())==5);
  CHECK(steps(e,5000,1)==CommandResult::Applied&&e.view().explorationCredit==10000-spent0&&availableBalls(e.view())==10);
  CHECK(e.view().encounter.id==first.id&&e.view().encounter.seed==first.seed);
  CHECK(e.view().explorationSpent==spent0&&e.view().explorationSpent+e.view().explorationCredit==e.view().totalSteps);
  CHECK(e.retreat()==CommandResult::Applied);
  const uint16_t next=e.view().encounterTarget;
  CHECK(e.acknowledgeResult(1,1)==CommandResult::Applied);
  CHECK(e.view().encounter.id==2&&e.view().explorationSpent==spent0+next);
  CHECK(e.view().explorationSpent+e.view().explorationCredit==e.view().totalSteps);
  CHECK(e.acknowledgeResult(1,2)==CommandResult::InvalidPhase);
  const uint16_t third=e.view().encounterTarget;
  CHECK(e.retreat()==CommandResult::Applied);CHECK(e.acknowledgeResult(2,1)==CommandResult::Applied);
  CHECK(e.view().encounter.id==3&&e.view().explorationCredit==10000-spent0-next-third);  auto s=fixture(1,7499);s->progress[0].walked=7499;
  MemoryStore threshold;threshold.disk=encoded(*s);Engine t(threshold);CHECK(t.begin()==CommandResult::Applied);
  threshold.before=true;CHECK(steps(t,1,1)==CommandResult::StorageError);
  CHECK(t.view().progress[0].walked==7499);threshold.before=false;CHECK(t.retrySave()==CommandResult::Applied);
  CHECK(t.evolutionView(1).options[0].status==CommandResult::Applied);
  const auto frozen=t.view().encounter;CHECK(t.evolve(request(t,1,2),clockFor(t))==CommandResult::Applied);
  CHECK(t.view().encounter.partner==2&&t.view().encounter.id==frozen.id&&t.view().encounter.seed==frozen.seed);
}
static void ballsAndRelease() {
  MemoryStore store;Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  CHECK(steps(e,1000,0)==CommandResult::Applied);
  CHECK(e.chooseStarter(1)==CommandResult::Applied&&e.view().phase==Phase::EncounterReady);
  win(e);
  const AttemptKey key(e.view().encounter.id,1);const auto old=store.disk;
  CHECK(e.beginThrow(key,0)==CommandResult::Applied&&availableBalls(e.view())==1&&store.disk==old);
  CHECK(e.giveUpCapture(key.encounterId)==CommandResult::InvalidPhase);
  store.before=true;CHECK(e.tick(e.throwView().targetMs+181,clockFor(e))==CommandResult::StorageError);
  CHECK(e.view().ballsSpent==0);const auto frozen=store.attempt;
  CHECK(e.pressThrow(key,0,clockFor(e))==CommandResult::SavePending);
  store.before=false;CHECK(e.retrySave()==CommandResult::Applied&&store.disk==frozen&&availableBalls(e.view())==1);
  CHECK(e.pressThrow(key,0,clockFor(e))==CommandResult::Duplicate&&e.view().ballsSpent==0);
  CHECK(e.giveUpCapture(key.encounterId)==CommandResult::Applied&&e.view().result==Result::Released);
  CHECK(e.giveUpCapture(key.encounterId)==CommandResult::Duplicate);
  CHECK(e.selectPartner(1)==CommandResult::InvalidPhase);
  CHECK(e.acknowledgeResult(key.encounterId,1)==CommandResult::Applied);
  while(e.view().phase==Phase::Walking)CHECK(steps(e,1,1)==CommandResult::Applied);
  win(e);
  CHECK(e.giveUpCapture(e.view().encounter.id)==CommandResult::Applied&&e.view().encounter.usedBalls==0);
  CHECK(e.giveUpCapture(key.encounterId)==CommandResult::InvalidInput);
  CHECK(e.view().ballsSpent==0);
}
static void recaptureAndGuards() {
  auto initial=fixture(1,8000);initial->progress[0].walked=8000;
  MemoryStore store;store.disk=encoded(*initial);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  const auto req=request(e,1,2);CHECK(e.evolve(req,clockFor(e))==CommandResult::Applied);
  uint32_t seed=0;for(;seed<1000000;++seed){Encounter encounter;encounter.id=1;encounter.seed=seed;
    if(chooseEncounter(e.view(),Route::Forest,seed)==1&&throwRoll(encounter,1,3)<98)break;}
  CHECK(seed<1000000);StepEvent event;event.batchId=1;event.clock=clockFor(e);event.partner=2;
  CHECK(e.observeSteps(&event,1,seed)==CommandResult::Applied);
  CHECK(e.view().encounter.species==1);
  CHECK(e.evolve(request(e,2,3),clockFor(e))==CommandResult::InsufficientProgress);
  win(e);CHECK(e.evolve(request(e,2,3),clockFor(e))==CommandResult::InvalidPhase);
  const AttemptKey first(1,1);CHECK(e.beginThrow(first,0)==CommandResult::Applied);
  CHECK(e.pressThrow(first,e.throwView().targetMs,clockFor(e))==CommandResult::Applied);
  CHECK(e.view().result==Result::Captured&&owns(e.view(),1)&&e.view().partner==2);
  CHECK(e.view().progress[0].walked-e.view().progress[0].spent==500&&e.view().captureCounts[0]==1);
  CHECK(e.view().rosterRevision==3&&e.view().firstCapture[0].at.totalSteps==8001);
  CHECK(e.acknowledgeResult(1,seed)==CommandResult::Applied);
  CHECK(e.selectPartner(1)==CommandResult::Applied&&e.view().rosterRevision==4);
  CHECK(e.evolve(req,clockFor(e))==CommandResult::Duplicate);
  const uint64_t revision=e.view().rosterRevision;
  for(seed=0;seed<10000;++seed) CHECK(chooseEncounter(e.view(),Route::Forest,seed)!=1);
  for(unsigned i=0;i<1999;++i){event.batchId=e.view().lastStepBatch+1;event.clock=clockFor(e);event.partner=1;
    CHECK(e.observeSteps(&event,1,seed)==CommandResult::Applied);}
  CHECK(e.view().encounter.species!=1);
  CHECK(e.view().captureCounts[0]==1&&e.view().rosterRevision==revision&&e.view().progress[0].spent==7500);
  CHECK(e.view().firstCapture[0].at.totalSteps==8001);
  auto pending=fixture(1,8000);pending->progress[0].walked=8000;
  pending->clock.pending.active=true;pending->clock.pending.transactionId=1;
  pending->clock.pending.targetUtcSeconds=1792627200;pending->clock.pending.targetDayKey=20261022;
  MemoryStore ps;ps.disk=encoded(*pending);Engine pe(ps);CHECK(pe.begin()==CommandResult::Applied);
  CHECK(pe.evolve(request(pe,1,2),clockFor(pe))==CommandResult::InvalidPhase);
  CHECK(pe.selectPartner(1)==CommandResult::InvalidPhase);
  MemoryStore bs;Engine be(bs);CHECK(be.begin()==CommandResult::Applied);CHECK(be.chooseStarter(1)==CommandResult::Applied);
  CHECK(steps(be,7500,1)==CommandResult::Applied);CHECK(be.startBattle(0)==CommandResult::Applied);
  CHECK(be.requestPause()==CommandResult::Applied);CHECK(be.tick(be.battleView().targetMs+181,clockFor(be))==CommandResult::Applied);
  CHECK(be.battleView().paused);CHECK(be.evolve(request(be,1,2),clockFor(be))==CommandResult::InvalidPhase);
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
  auto bad=fixture(1,1000);bad->progress[0].walked=1001;CHECK(!validate(*bad));bad->progress[0].walked=1000;bad->progress[0].spent=1001;CHECK(!validate(*bad));
  bad->progress[0].spent=0;bad->ballsSpent=2;CHECK(!validate(*bad));bad->ballsSpent=0;bad->rewardBaseSteps=1001;CHECK(!validate(*bad));
  bad->rewardBaseSteps=0;bad->registered[0]=0;CHECK(!validate(*bad));
}
int main(){allEdges();johtoRules();accountsAndFailure();stepThresholds();ballsAndRelease();recaptureAndGuards();limitsAndValidation();std::printf("progression: %u checks passed\n",checks);}
