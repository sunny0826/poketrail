#include "driver.hpp"

namespace simulator { namespace e2e {
void scenarioMigration(Driver& d,const std::string& phase) {
  d.check(d.save().schema==6,"legacy bytes upgraded to schema6");
  if(phase=="activity") {
    d.check(d.save().totalSteps==6293 && d.save().explorationCredit==1293,"frozen activity archive preserves accounting");
    d.check(d.save().phase==Phase::Walking && d.save().encounter.id==0,"obsolete activity encounter retired");
    d.state("activity-upgraded"); d.shot("activity-upgraded"); d.close(); return;
  }
  if(phase=="rule2" || phase=="rule2-reload") {
    d.check(d.save().encounter.ruleVersion==3 && d.save().ballsSpent==0,"historical rule2 upgraded to unlimited rule3");
    if(phase=="rule2") {
      d.check(d.save().encounter.id==1 && d.save().totalSteps==5000,"historical encounter and accounts preserved");
      d.throwBall(false); d.finishCapture(); d.throwBall(false); d.finishCapture();
      d.check(d.save().encounter.usedBalls==2 && d.save().result==Result::Escaped && d.save().ballsSpent==0,"upgraded throws do not debit inventory");
      d.select(0); d.walk(600);
    }
    d.check(d.save().phase==Phase::EncounterReady && d.save().encounter.id==2 && d.save().encounter.ruleVersion==3,"next rule3 encounter persists");
    d.state("rule2-upgraded"); d.shot("rule2-upgraded"); d.close(); return;
  }
  if(phase=="reload") {
    d.check(d.save().encounter.ruleVersion==3,"new encounter uses current unlimited-ball rule");
    d.check(d.save().ballsSpent==1,"legacy charge persisted exactly once");
    d.state("migration-reloaded"); d.close(); return;
  }
  d.check(d.save().encounter.ruleVersion==1 && d.save().encounter.usedBalls==1,"frozen v1 in-flight encounter retains old attempt");
  d.check(availableBalls(d.save())==0,"legacy inventory begins empty");
  d.tap(233,238);
  d.check(!d.runtime().engine().throwView().active && d.screen().command==CommandResult::NoBalls,"old encounter rejects throw without inventory");
  d.tap(233,286);
  d.check(d.screen().page==Page::Home,"old encounter can be deferred");
  d.walk(1000);
  d.check(availableBalls(d.save())==1,"walking replenishes legacy inventory");
  d.homeMenu(); d.select(0); d.throwBall(false); d.finishCapture();
  d.check(d.save().ballsSpent==1 && d.save().encounter.usedBalls==2 && d.save().result==Result::Escaped,"second legacy miss charges exactly one ball and escapes");
  d.check(d.save().encounter.attempts[1].ruleVersion==1,"new legacy attempt retains rule1");
  d.select(0); d.walk(600);
  d.check(d.save().encounter.ruleVersion==3 && d.save().phase==Phase::EncounterReady,"next encounter starts with rule3");
  d.state("migration-final"); d.shot("migration-final"); d.close();
}
} }
